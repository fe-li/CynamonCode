#include "ui/MainWindow.h"

#include "app/AppInfo.h"
#include "editor/EditorWidget.h"
#include "services/LanguageRegistry.h"
#include "services/SearchService.h"
#include "ui/AboutDialog.h"
#include "ui/FileTreeDock.h"
#include "ui/FindReplaceDialog.h"
#include "ui/SearchResultsDock.h"

#include <Qsci/qscidocument.h>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace cynamoncode {

namespace {

// QTabBar traktuje „&” jako skrót klawiszowy — w nazwach plików trzeba go podwoić.
QString escapeMnemonic(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

constexpr int SearchPreviewLength = 200;
// Panel wyników pokazuje najwyżej tyle trafień (liczba w podsumowaniu jest pełna).
constexpr int MaxDisplayedHits = 5000;

} // namespace

MainWindow::MainWindow(std::unique_ptr<UserPrompts> prompts, std::unique_ptr<QSettings> settings,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_prompts(std::move(prompts))
    , m_settingsStore(std::move(settings))
{
    if (!m_prompts)
        m_prompts = std::make_unique<DialogPrompts>();
    if (m_settingsStore) {
        m_settingsService = std::make_unique<SettingsService>(*m_settingsStore);
        m_editorSettings = m_settingsService->load();
    }
    m_theme = m_editorSettings.theme == QLatin1String("dark") ? ThemeId::Dark : ThemeId::Light;

    setObjectName(QStringLiteral("MainWindow"));
    setWindowIcon(applicationIcon());
    resize(1024, 720);
    if (m_settingsService) {
        const QByteArray geometry = m_settingsService->windowGeometry();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
    }

    createPanes();

    const auto addStatusLabel = [this](const char *name) {
        auto *label = new QLabel(this);
        label->setObjectName(QLatin1String(name));
        statusBar()->addPermanentWidget(label);
        return label;
    };
    m_cursorLabel = addStatusLabel("cursorLabel");
    m_languageLabel = addStatusLabel("languageLabel");
    m_encodingLabel = addStatusLabel("encodingLabel");
    m_lineEndingLabel = addStatusLabel("lineEndingLabel");

    m_fileTree = new FileTreeDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileTree);
    m_fileTree->hide();
    connect(m_fileTree, &FileTreeDock::fileActivated, this,
            [this](const QString &path) { openFile(path); });

    m_searchResults = new SearchResultsDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_searchResults);
    m_searchResults->hide();
    connect(m_searchResults, &SearchResultsDock::hitActivated, this, &MainWindow::showSearchHit);

    createActions();
    createMenus();
    createViewAndLanguageMenus();

    connect(&m_fileWatcher, &FileWatchService::fileChanged, this,
            &MainWindow::onWatchedFileChanged);
    // Aktywny panel = ten, w którym jest fokus klawiatury.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        auto *editor = qobject_cast<EditorWidget *>(now);
        const int pane = editor ? paneOf(editor) : -1;
        if (pane >= 0 && pane != m_activePane) {
            m_activePane = pane;
            onCurrentEditorChanged();
        }
    });

    newDocument();
}

MainWindow::~MainWindow()
{
    // Sloty korzystają z pól tej klasy — odłącz je, zanim Qt zniszczy karty.
    disconnect(qApp, nullptr, this, nullptr);
    for (QTabWidget *pane : m_panes) {
        pane->disconnect(this);
        pane->tabBar()->disconnect(this);
    }
    for (EditorWidget *editor : allEditors())
        editor->disconnect(this);
    delete m_splitter;
    m_splitter = nullptr;
    m_panes = {};
}

void MainWindow::createPanes()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName(QStringLiteral("viewSplitter"));
    m_splitter->setChildrenCollapsible(false);

    for (int index = 0; index < PaneCount; ++index) {
        auto *tabs = new QTabWidget(m_splitter);
        tabs->setObjectName(index == 0 ? QStringLiteral("documentTabs")
                                       : QStringLiteral("documentTabs2"));
        tabs->setTabsClosable(true);
        tabs->setMovable(true);
        tabs->setDocumentMode(true);
        tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        m_panes[static_cast<size_t>(index)] = tabs;

        connect(tabs, &QTabWidget::tabCloseRequested, this, [this, tabs](int tab) {
            closeEditor(qobject_cast<EditorWidget *>(tabs->widget(tab)));
        });
        connect(tabs, &QTabWidget::currentChanged, this, [this, index, tabs] {
            // Aktywny panel podąża za fokusem: zmiana karty w panelu bez fokusu
            // (np. zamknięcie karty „x”) nie przenosi skrótów do innego dokumentu.
            QWidget *focus = QApplication::focusWidget();
            if (tabs->count() > 0 && (!focus || tabs->isAncestorOf(focus)))
                m_activePane = index;
            onCurrentEditorChanged();
        });
        // Kliknięcie karty: panel staje się aktywny, fokus trafia do jej edytora.
        connect(tabs->tabBar(), &QTabBar::tabBarClicked, this, [this, tabs](int tab) {
            if (auto *editor = qobject_cast<EditorWidget *>(tabs->widget(tab)))
                QTimer::singleShot(0, this, [this, editor] {
                    if (paneOf(editor) >= 0)
                        activateEditor(editor);
                });
        });
        connect(tabs->tabBar(), &QTabBar::customContextMenuRequested, this,
                [this, index](const QPoint &position) { showTabMenu(index, position); });
    }
    m_panes[1]->hide(); // drugi panel tylko, gdy ma karty (D036)
    setCentralWidget(m_splitter);
}

bool MainWindow::openFile(const QString &path, int pane)
{
    const int target = targetPane(pane);
    EditorWidget *pristine =
        (documentCount() == 1 && isPristineUntitled(editorAt(0))) ? editorAt(0) : nullptr;

    // Duży plik: pytanie przed odczytem (D039). Otwarty już plik tylko aktywujemy.
    const QFileInfo info(path);
    if (!m_documents.findOpen(path) && info.isFile()
        && info.size() > DocumentController::LargeFileWarningBytes
        && !m_prompts->askOpenLargeFile(this, QDir::toNativeSeparators(path), info.size()))
        return false;

    const OpenOutcome outcome = m_documents.open(path);
    switch (outcome.status) {
    case OpenStatus::AlreadyOpen:
        if (EditorWidget *existing = viewInPane(outcome.id, m_activePane)) {
            activateEditor(existing);
        } else if (EditorWidget *any = findEditor(outcome.id)) {
            activateEditor(any);
        }
        return true;
    case OpenStatus::Failed:
        reportOpenFailure(path, outcome);
        return false;
    case OpenStatus::Opened:
        break;
    }

    const DocumentState *document = m_documents.document(outcome.id);
    EditorWidget *editor = addEditorTab(outcome.id, outcome.text, document->lineEnding, target);
    if (!editor) {
        m_documents.close(outcome.id);
        m_prompts->showError(this, tr("Nie można otworzyć pliku"),
                             tr("%1\n\nTreści nie udało się wczytać do edytora bez zmian. "
                                "Otwieranie anulowano; plik pozostał nietknięty.")
                                 .arg(QDir::toNativeSeparators(path)));
        return false;
    }

    m_fileWatcher.watch(document->filePath);

    // Jak w typowych edytorach: pusta, nieruszona karta „Nowy” ustępuje otwartemu plikowi.
    if (pristine)
        closeEditor(pristine);

    // Przy przywracaniu sesji bez pytań o końce linii — plik był już otwarty wcześniej.
    if (document->lineEnding == LineEnding::Mixed && !m_restoringSession)
        handleMixedLineEndings(editor);
    return true;
}

bool MainWindow::moveCurrentToOtherView()
{
    EditorWidget *editor = currentEditor();
    if (!editor)
        return false;
    const int source = paneOf(editor);
    const int destination = 1 - source;
    const DocumentId id = documentIdOf(editor);

    // Dokument ma już widok w drugim panelu (klon) — zostaje tylko tamten.
    if (EditorWidget *existing = viewInPane(id, destination)) {
        removeView(editor);
        activateEditor(existing);
        return true;
    }
    QTabWidget *from = m_panes[static_cast<size_t>(source)];
    from->removeTab(from->indexOf(editor));
    insertIntoPane(editor, destination);
    return true;
}

bool MainWindow::cloneCurrentToOtherView()
{
    EditorWidget *editor = currentEditor();
    if (!editor)
        return false;
    const int destination = 1 - paneOf(editor);
    const DocumentId id = documentIdOf(editor);
    if (EditorWidget *existing = viewInPane(id, destination)) {
        activateEditor(existing);
        return true;
    }

    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    EditorWidget *clone = createEditor(id, editor);
    insertIntoPane(clone, destination);
    clone->setCursorPosition(line, index);
    return true;
}

void MainWindow::setTheme(ThemeId theme)
{
    m_theme = theme;
    const ThemePalette palette = ThemePalette::forId(theme);
    for (EditorWidget *editor : allEditors())
        editor->setTheme(palette);
    m_lightThemeAction->setChecked(theme == ThemeId::Light);
    m_darkThemeAction->setChecked(theme == ThemeId::Dark);
    changeSettings([theme](EditorSettings &settings) {
        settings.theme = theme == ThemeId::Dark ? QStringLiteral("dark") : QStringLiteral("light");
    });
}

void MainWindow::setZoom(int zoom)
{
    zoom = std::clamp(zoom, SettingsService::MinZoom, SettingsService::MaxZoom);
    if (zoom == m_editorSettings.zoom)
        return;
    changeSettings([zoom](EditorSettings &settings) { settings.zoom = zoom; });
}

QFont MainWindow::editorFont() const
{
    QFont font = m_editorSettings.fontFamily.isEmpty()
                     ? QFontDatabase::systemFont(QFontDatabase::FixedFont)
                     : QFont(m_editorSettings.fontFamily);
    if (m_editorSettings.fontPointSize > 0)
        font.setPointSize(m_editorSettings.fontPointSize);
    return font;
}

void MainWindow::applyEditorSettings(EditorWidget *editor) const
{
    const QFont font = editorFont();
    if (editor->editorFont() != font)
        editor->setEditorFont(font);
    editor->setWrapMode(m_editorSettings.wordWrap ? QsciScintilla::WrapWord
                                                  : QsciScintilla::WrapNone);
    editor->setLineNumbersVisible(m_editorSettings.lineNumbers);
    editor->setWhitespaceVisibility(m_editorSettings.showWhitespace ? QsciScintilla::WsVisible
                                                                    : QsciScintilla::WsInvisible);
    editor->setEolVisibility(m_editorSettings.showLineEndings);
    if (editor->SendScintilla(QsciScintillaBase::SCI_GETZOOM) != m_editorSettings.zoom)
        editor->zoomTo(m_editorSettings.zoom);
}

void MainWindow::changeSettings(const std::function<void(EditorSettings &)> &change)
{
    change(m_editorSettings);
    for (EditorWidget *editor : allEditors())
        applyEditorSettings(editor);
    if (m_settingsService)
        m_settingsService->save(m_editorSettings);

    if (m_wordWrapAction) {
        m_wordWrapAction->setChecked(m_editorSettings.wordWrap);
        m_lineNumbersAction->setChecked(m_editorSettings.lineNumbers);
        m_whitespaceAction->setChecked(m_editorSettings.showWhitespace);
        m_lineEndingsAction->setChecked(m_editorSettings.showLineEndings);
    }
    if (m_restoreSessionAction)
        m_restoreSessionAction->setChecked(m_editorSettings.restoreSession);
}

void MainWindow::chooseFont()
{
    if (const std::optional<QFont> font = m_prompts->askFont(this, editorFont())) {
        changeSettings([&font](EditorSettings &settings) {
            settings.fontFamily = font->family();
            settings.fontPointSize = font->pointSize() > 0 ? font->pointSize() : 0;
        });
    }
}

int MainWindow::documentCount() const
{
    int count = 0;
    for (QTabWidget *tabs : m_panes)
        count += tabs->count();
    return count;
}

EditorWidget *MainWindow::currentEditor() const
{
    QTabWidget *active = m_panes[static_cast<size_t>(m_activePane)];
    if (active->count() == 0)
        active = m_panes[static_cast<size_t>(1 - m_activePane)];
    return qobject_cast<EditorWidget *>(active->currentWidget());
}

EditorWidget *MainWindow::editorAt(int index) const
{
    return allEditors().value(index, nullptr);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Każdy dokument raz, niezależnie od liczby widoków.
    for (const DocumentId id : openDocuments()) {
        if (!resolveUnsavedChanges(findEditor(id))) {
            event->ignore();
            return;
        }
    }
    if (m_settingsService)
        m_settingsService->saveWindowGeometry(saveGeometry());
    saveSession();
    event->accept();
}

void MainWindow::createActions()
{
    const auto makeAction = [this](const QString &text, const char *name,
                                   const QKeySequence &shortcut = QKeySequence()) {
        auto *action = new QAction(text, this);
        action->setObjectName(QLatin1String(name));
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        return action;
    };

    m_newAction = makeAction(tr("&Nowy"), "actionNew", QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(m_newAction, &QAction::triggered, this, [this] { newDocument(); });

    m_openAction = makeAction(tr("&Otwórz…"), "actionOpen", QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(m_openAction, &QAction::triggered, this, [this] { openFilesFromDialog(); });

    m_openFolderAction = makeAction(tr("Otwórz &folder…"), "actionOpenFolder",
                                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_openFolderAction, &QAction::triggered, this, [this] { openFolderFromDialog(); });

    m_saveAction = makeAction(tr("&Zapisz"), "actionSave", QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(m_saveAction, &QAction::triggered, this, [this] { saveCurrent(); });

    m_saveAsAction = makeAction(tr("Zapisz &jako…"), "actionSaveAs",
                                QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_saveAsAction, &QAction::triggered, this, [this] { saveCurrentAs(); });

    m_saveAllAction = makeAction(tr("Zapisz &wszystkie"), "actionSaveAll");
    connect(m_saveAllAction, &QAction::triggered, this, [this] { saveAll(); });

    m_closeTabAction =
        makeAction(tr("Zamknij &kartę"), "actionCloseTab", QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(m_closeTabAction, &QAction::triggered, this, [this] { closeCurrentTab(); });

    // Jawny skrót: QKeySequence::Quit bywa pusty zależnie od motywu platformy.
    m_quitAction = makeAction(tr("Za&kończ"), "actionQuit", QKeySequence(Qt::CTRL | Qt::Key_Q));
    m_quitAction->setStatusTip(tr("Zamyka program"));
    // close(), a nie QApplication::quit(): closeEvent pyta o niezapisane zmiany.
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_undoAction = makeAction(tr("&Cofnij"), "actionUndo", QKeySequence(Qt::CTRL | Qt::Key_Z));
    connect(m_undoAction, &QAction::triggered, this, [this] {
        if (EditorWidget *editor = currentEditor())
            editor->undo();
    });

    m_redoAction = makeAction(tr("&Ponów"), "actionRedo");
    m_redoAction->setShortcuts(
        {QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(m_redoAction, &QAction::triggered, this, [this] {
        if (EditorWidget *editor = currentEditor())
            editor->redo();
    });

    m_cutAction = makeAction(tr("Wy&tnij"), "actionCut", QKeySequence(Qt::CTRL | Qt::Key_X));
    connect(m_cutAction, &QAction::triggered, this, [this] {
        if (EditorWidget *editor = currentEditor())
            editor->cut();
    });

    m_copyAction = makeAction(tr("&Kopiuj"), "actionCopy", QKeySequence(Qt::CTRL | Qt::Key_C));
    connect(m_copyAction, &QAction::triggered, this, [this] {
        if (EditorWidget *editor = currentEditor())
            editor->copy();
    });

    m_pasteAction = makeAction(tr("&Wklej"), "actionPaste", QKeySequence(Qt::CTRL | Qt::Key_V));
    connect(m_pasteAction, &QAction::triggered, this, [this] {
        if (EditorWidget *editor = currentEditor())
            editor->paste();
    });

    m_selectAllAction =
        makeAction(tr("Zaznacz &wszystko"), "actionSelectAll", QKeySequence(Qt::CTRL | Qt::Key_A));
    connect(m_selectAllAction, &QAction::triggered, this, [this] {
        if (EditorWidget *editor = currentEditor())
            editor->selectAll();
    });

    m_goToLineAction = makeAction(tr("&Przejdź do wiersza…"), "actionGoToLine",
                                  QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(m_goToLineAction, &QAction::triggered, this, [this] { goToLine(); });

    m_moveToOtherViewAction =
        makeAction(tr("&Przenieś do drugiego widoku"), "actionMoveToOtherView");
    connect(m_moveToOtherViewAction, &QAction::triggered, this,
            [this] { moveCurrentToOtherView(); });

    m_cloneToOtherViewAction =
        makeAction(tr("&Sklonuj do drugiego widoku"), "actionCloneToOtherView");
    connect(m_cloneToOtherViewAction, &QAction::triggered, this,
            [this] { cloneCurrentToOtherView(); });
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Plik"));
    fileMenu->setObjectName(QStringLiteral("menuFile"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_openFolderAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addAction(m_saveAllAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_closeTabAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edycja"));
    editMenu->setObjectName(QStringLiteral("menuEdit"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);

    createSearchActions();
    QMenu *searchMenu = menuBar()->addMenu(tr("&Szukaj"));
    searchMenu->setObjectName(QStringLiteral("menuSearch"));
    searchMenu->addAction(m_findAction);
    searchMenu->addAction(m_replaceAction);
    searchMenu->addAction(m_findNextAction);
    searchMenu->addAction(m_findPreviousAction);
    searchMenu->addAction(m_findInAllAction);
    searchMenu->addSeparator();
    searchMenu->addAction(m_goToLineAction);
}

void MainWindow::createSearchActions()
{
    m_findAction = new QAction(tr("&Znajdź…"), this);
    m_findAction->setObjectName(QStringLiteral("actionFind"));
    m_findAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(m_findAction, &QAction::triggered, this, [this] { openFindDialog(false); });

    m_replaceAction = new QAction(tr("Za&mień…"), this);
    m_replaceAction->setObjectName(QStringLiteral("actionReplace"));
    m_replaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    connect(m_replaceAction, &QAction::triggered, this, [this] { openFindDialog(true); });

    m_findNextAction = new QAction(tr("Znajdź &następny"), this);
    m_findNextAction->setObjectName(QStringLiteral("actionFindNext"));
    m_findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(m_findNextAction, &QAction::triggered, this, [this] { findFromMenu(true); });

    m_findPreviousAction = new QAction(tr("Znajdź &poprzedni"), this);
    m_findPreviousAction->setObjectName(QStringLiteral("actionFindPrevious"));
    m_findPreviousAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(m_findPreviousAction, &QAction::triggered, this, [this] { findFromMenu(false); });

    m_findInAllAction = new QAction(tr("Znajdź we &wszystkich otwartych…"), this);
    m_findInAllAction->setObjectName(QStringLiteral("actionFindInAll"));
    m_findInAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(m_findInAllAction, &QAction::triggered, this, [this] { findInAllFromMenu(); });
}

FindReplaceDialog *MainWindow::findDialog()
{
    if (!m_findDialog) {
        m_findDialog = new FindReplaceDialog(this);
        connect(m_findDialog, &FindReplaceDialog::findRequested, this,
                [this](bool forward) { findInCurrent(m_findDialog->options(), forward); });
        connect(m_findDialog, &FindReplaceDialog::replaceRequested, this, [this] {
            replaceInCurrent(m_findDialog->options(), m_findDialog->replacement());
        });
        connect(m_findDialog, &FindReplaceDialog::replaceAllRequested, this, [this] {
            replaceAllInCurrent(m_findDialog->options(), m_findDialog->replacement());
        });
        connect(m_findDialog, &FindReplaceDialog::findInAllRequested, this,
                [this] { findInAllDocuments(m_findDialog->options()); });
        connect(m_findDialog, &FindReplaceDialog::replaceInAllRequested, this, [this] {
            replaceInAllDocuments(m_findDialog->options(), m_findDialog->replacement());
        });
    }
    return m_findDialog;
}

void MainWindow::openFindDialog(bool replace)
{
    FindReplaceDialog *dialog = findDialog();
    if (EditorWidget *editor = currentEditor()) {
        const QString selected = editor->selectedText();
        if (!selected.isEmpty() && !selected.contains(QLatin1Char('\n'))
            && !selected.contains(QLatin1Char('\r')))
            dialog->setPattern(selected);
    }
    if (replace)
        dialog->showForReplace();
    else
        dialog->showForFind();
}

void MainWindow::findFromMenu(bool forward)
{
    if (!m_findDialog || !m_findDialog->hasPattern()) {
        openFindDialog(false);
        return;
    }
    findInCurrent(m_findDialog->options(), forward);
}

void MainWindow::findInAllFromMenu()
{
    if (!m_findDialog || !m_findDialog->hasPattern()) {
        openFindDialog(false);
        return;
    }
    findInAllDocuments(m_findDialog->options());
}

bool MainWindow::findInCurrent(const SearchOptions &options, bool forward)
{
    EditorWidget *editor = currentEditor();
    if (!editor || options.pattern.isEmpty())
        return false;
    if (const QString error = SearchService::patternError(options); !error.isEmpty()) {
        reportSearchStatus(error);
        return false;
    }

    const QString text = editor->text();
    const auto [selectionStart, selectionEnd] = editor->selectionRange(text);
    const std::optional<SearchMatch> match =
        SearchService::find(text, options, forward ? selectionEnd : selectionStart, forward);
    if (!match) {
        reportSearchStatus(tr("Nie znaleziono „%1”.").arg(options.pattern));
        return false;
    }
    editor->selectRange(text, match->start, match->length);
    reportSearchStatus(QString());
    return true;
}

bool MainWindow::replaceInCurrent(const SearchOptions &options, const QString &replacement)
{
    EditorWidget *editor = currentEditor();
    if (!editor || options.pattern.isEmpty())
        return false;
    if (const QString error = SearchService::patternError(options); !error.isEmpty()) {
        reportSearchStatus(error);
        return false;
    }

    // Zaznaczenie będące dopasowaniem zostaje zamienione; potem następne wystąpienie.
    const QString text = editor->text();
    const auto [selectionStart, selectionEnd] = editor->selectionRange(text);
    const std::optional<SearchMatch> match = SearchService::matchAt(text, options, selectionStart);
    if (match && selectionEnd - selectionStart == match->length) {
        const QString inserted =
            SearchService::replacementFor(text, options, selectionStart, replacement);
        editor->applyReplacements(text, {{selectionStart, match->length, inserted}});
        editor->selectRange(editor->text(), selectionStart + inserted.size(), 0);
    }
    return findInCurrent(options, true);
}

int MainWindow::replaceAllInCurrent(const SearchOptions &options, const QString &replacement)
{
    EditorWidget *editor = currentEditor();
    if (!editor || options.pattern.isEmpty())
        return 0;
    if (const QString error = SearchService::patternError(options); !error.isEmpty()) {
        reportSearchStatus(error);
        return 0;
    }

    const QString text = editor->text();
    const QList<Replacement> replacements = SearchService::replaceAll(text, options, replacement);
    editor->applyReplacements(text, replacements);
    reportSearchStatus(replacements.isEmpty()
                           ? tr("Nie znaleziono „%1”.").arg(options.pattern)
                           : tr("Liczba zamian: %1.").arg(replacements.size()));
    return static_cast<int>(replacements.size());
}

int MainWindow::findInAllDocuments(const SearchOptions &options)
{
    if (options.pattern.isEmpty())
        return 0;
    if (const QString error = SearchService::patternError(options); !error.isEmpty()) {
        reportSearchStatus(error);
        return 0;
    }

    m_searchResults->clearResults(QString());
    int total = 0;
    int documents = 0;
    int displayed = 0;
    for (const DocumentId id : openDocuments()) {
        const QString text = findEditor(id)->text();
        const qsizetype count = SearchService::countMatches(text, options);
        if (count == 0)
            continue;
        total += static_cast<int>(count);
        ++documents;
        // Do panelu tylko tyle trafień, ile zostało w limicie (pamięć i czas, D039).
        const QList<SearchMatch> matches =
            SearchService::findAll(text, options, std::max(MaxDisplayedHits - displayed, 0));
        displayed += static_cast<int>(matches.size());

        // Numery wierszy przyrostowo, jak w Scintilli: LF, CRLF i samotne CR.
        const auto isLineBreak = [&text](qsizetype i) {
            const QChar c = text.at(i);
            return c == QLatin1Char('\n')
                   || (c == QLatin1Char('\r')
                       && (i + 1 >= text.size() || text.at(i + 1) != QLatin1Char('\n')));
        };
        QList<SearchHit> hits;
        hits.reserve(matches.size());
        int line = 0;
        qsizetype scanned = 0;
        qsizetype lineStart = 0;
        qsizetype lineEnd = -1; // koniec bieżącego wiersza, liczony raz na wiersz
        for (const SearchMatch &match : matches) {
            for (; scanned < match.start; ++scanned) {
                if (isLineBreak(scanned)) {
                    ++line;
                    lineStart = scanned + 1;
                    lineEnd = -1;
                }
            }
            if (lineEnd < match.start) {
                lineEnd = match.start;
                while (lineEnd < text.size() && text.at(lineEnd) != QLatin1Char('\n')
                       && text.at(lineEnd) != QLatin1Char('\r'))
                    ++lineEnd;
            }
            // Podgląd: fragment wokół trafienia (długie, np. zminifikowane wiersze).
            const qsizetype from = std::max(lineStart, match.start - SearchPreviewLength / 4);
            const qsizetype to = std::min(lineEnd, from + SearchPreviewLength);
            QString preview = text.mid(from, to - from).trimmed();
            if (from > lineStart)
                preview.prepend(QStringLiteral("…"));
            if (to < lineEnd)
                preview.append(QStringLiteral("…"));
            hits.append({match.start, match.length, line, preview});
        }
        m_searchResults->addDocument(id, displayName(id), static_cast<int>(count), hits);
    }

    QString summary =
        total > 0 ? tr("Liczba wystąpień „%1”: %2 (dokumenty: %3).")
                        .arg(options.pattern)
                        .arg(total)
                        .arg(documents)
                  : tr("Nie znaleziono „%1” w otwartych dokumentach.").arg(options.pattern);
    if (total > displayed)
        summary += QLatin1Char(' ') + tr("Pokazano pierwsze %1.").arg(displayed);
    m_searchResults->setSummary(summary);
    m_searchResults->show();
    m_searchResults->raise();
    reportSearchStatus(summary);
    return total;
}

int MainWindow::replaceInAllDocuments(const SearchOptions &options, const QString &replacement)
{
    if (options.pattern.isEmpty())
        return 0;
    if (const QString error = SearchService::patternError(options); !error.isEmpty()) {
        reportSearchStatus(error);
        return 0;
    }

    int matches = 0;
    int documents = 0;
    for (const DocumentId id : openDocuments()) {
        const qsizetype count = SearchService::countMatches(findEditor(id)->text(), options);
        if (count > 0) {
            matches += static_cast<int>(count);
            ++documents;
        }
    }
    if (matches == 0) {
        reportSearchStatus(tr("Nie znaleziono „%1” w otwartych dokumentach.").arg(options.pattern));
        return 0;
    }
    if (!m_prompts->askReplaceInAll(this, matches, documents))
        return 0;

    // Po pytaniu (zagnieżdżona pętla zdarzeń) liczymy od nowa na aktualnym tekście.
    int total = 0;
    int changed = 0;
    for (const DocumentId id : openDocuments()) {
        EditorWidget *editor = findEditor(id);
        const QString text = editor->text();
        const QList<Replacement> replacements =
            SearchService::replaceAll(text, options, replacement);
        if (replacements.isEmpty())
            continue;
        editor->applyReplacements(text, replacements); // jedno cofnięcie na dokument
        total += static_cast<int>(replacements.size());
        ++changed;
    }
    reportSearchStatus(tr("Liczba zamian: %1 (dokumenty: %2).").arg(total).arg(changed));
    return total;
}

void MainWindow::showSearchHit(DocumentId id, qsizetype start, qsizetype length)
{
    EditorWidget *editor = viewInPane(id, m_activePane);
    if (!editor)
        editor = findEditor(id);
    if (!editor) {
        reportSearchStatus(tr("Dokument z wyników wyszukiwania został zamknięty."));
        return;
    }
    activateEditor(editor);
    const QString text = editor->text();
    // Tekst mógł się zmienić od wyszukiwania — pozycje przycinamy do bieżącej treści.
    const qsizetype safeStart = std::clamp<qsizetype>(start, 0, text.size());
    const qsizetype safeLength = std::clamp<qsizetype>(length, 0, text.size() - safeStart);
    editor->selectRange(text, safeStart, safeLength);
}

void MainWindow::reportSearchStatus(const QString &message)
{
    if (m_findDialog)
        m_findDialog->setStatus(message);
    if (message.isEmpty())
        statusBar()->clearMessage();
    else
        statusBar()->showMessage(message, 5000);
}

void MainWindow::openFolder(const QString &folder)
{
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        return;
    m_fileTree->setRootFolder(folder);
    m_fileTree->show();
    m_fileTree->raise();
}

QString MainWindow::workspaceFolder() const
{
    return m_fileTree->rootFolder();
}

void MainWindow::openFolderFromDialog()
{
    const QString current = workspaceFolder().isEmpty() ? QDir::homePath() : workspaceFolder();
    const QString folder = m_prompts->askFolder(this, current);
    if (!folder.isEmpty())
        openFolder(folder);
}

void MainWindow::createViewAndLanguageMenus()
{
    QMenu *viewMenu = menuBar()->addMenu(tr("&Widok"));
    viewMenu->setObjectName(QStringLiteral("menuView"));
    QMenu *themeMenu = viewMenu->addMenu(tr("&Motyw"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    m_lightThemeAction = themeMenu->addAction(tr("&Jasny"));
    m_lightThemeAction->setObjectName(QStringLiteral("actionThemeLight"));
    m_darkThemeAction = themeMenu->addAction(tr("&Ciemny"));
    m_darkThemeAction->setObjectName(QStringLiteral("actionThemeDark"));
    for (QAction *action : {m_lightThemeAction, m_darkThemeAction}) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }
    m_lightThemeAction->setChecked(m_theme == ThemeId::Light);
    m_darkThemeAction->setChecked(m_theme == ThemeId::Dark);
    connect(m_lightThemeAction, &QAction::triggered, this, [this] { setTheme(ThemeId::Light); });
    connect(m_darkThemeAction, &QAction::triggered, this, [this] { setTheme(ThemeId::Dark); });
    createSettingsActions(viewMenu);

    viewMenu->addSeparator();
    QAction *fileTreeToggle = m_fileTree->toggleViewAction();
    fileTreeToggle->setText(tr("Panel &plików"));
    fileTreeToggle->setObjectName(QStringLiteral("actionToggleFileTree"));
    viewMenu->addAction(fileTreeToggle);
    QAction *resultsToggle = m_searchResults->toggleViewAction();
    resultsToggle->setText(tr("&Wyniki wyszukiwania"));
    resultsToggle->setObjectName(QStringLiteral("actionToggleSearchResults"));
    viewMenu->addAction(resultsToggle);
    viewMenu->addSeparator();
    viewMenu->addAction(m_moveToOtherViewAction);
    viewMenu->addAction(m_cloneToOtherViewAction);

    QMenu *languageMenu = menuBar()->addMenu(tr("&Język"));
    languageMenu->setObjectName(QStringLiteral("menuLanguage"));
    m_languageGroup = new QActionGroup(this);
    // ExclusiveOptional: bez otwartej karty żaden język nie jest zaznaczony.
    m_languageGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (const LanguageId language : LanguageRegistry::all()) {
        QAction *action = languageMenu->addAction(LanguageRegistry::displayName(language));
        action->setObjectName(QStringLiteral("actionLanguage_%1").arg(LanguageRegistry::key(language)));
        action->setCheckable(true);
        action->setData(static_cast<int>(language));
        m_languageGroup->addAction(action);
        m_languageActions.append(action);
        connect(action, &QAction::triggered, this, [this, language] { chooseLanguage(language); });
        if (language == LanguageId::Plain)
            languageMenu->addSeparator();
    }

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Ustawienia"));
    settingsMenu->setObjectName(QStringLiteral("menuSettings"));
    settingsMenu->addAction(m_fontAction);
    m_restoreSessionAction = settingsMenu->addAction(tr("&Przywracaj sesję przy starcie"));
    m_restoreSessionAction->setObjectName(QStringLiteral("actionRestoreSession"));
    m_restoreSessionAction->setCheckable(true);
    m_restoreSessionAction->setChecked(m_editorSettings.restoreSession);
    connect(m_restoreSessionAction, &QAction::toggled, this, [this](bool on) {
        changeSettings([on](EditorSettings &settings) { settings.restoreSession = on; });
        // Wyłączenie usuwa zapamiętaną sesję programu — bez śladu ostatnio otwartych plików.
        if (!on && !sessionFilePath().isEmpty())
            QFile::remove(sessionFilePath());
    });

    QMenu *helpMenu = menuBar()->addMenu(tr("Pomo&c"));
    helpMenu->setObjectName(QStringLiteral("menuHelp"));
    QAction *aboutAction = helpMenu->addAction(tr("&O programie %1…").arg(AppInfo::productName()));
    aboutAction->setObjectName(QStringLiteral("actionAbout"));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, [this] {
        if (!m_aboutDialog)
            m_aboutDialog = new AboutDialog(this);
        m_aboutDialog->open(); // modalne dla okna, bez blokującej pętli exec()
    });
}

void MainWindow::createSettingsActions(QMenu *viewMenu)
{
    const auto makeToggle = [this, viewMenu](const QString &text, const char *name, bool checked,
                                             auto setter) {
        auto *action = viewMenu->addAction(text);
        action->setObjectName(QLatin1String(name));
        action->setCheckable(true);
        action->setChecked(checked);
        connect(action, &QAction::toggled, this, [this, setter](bool on) {
            changeSettings([setter, on](EditorSettings &settings) { setter(settings, on); });
        });
        return action;
    };

    viewMenu->addSeparator();
    m_wordWrapAction = makeToggle(tr("&Zawijaj wiersze"), "actionWordWrap",
                                  m_editorSettings.wordWrap,
                                  [](EditorSettings &s, bool on) { s.wordWrap = on; });
    m_lineNumbersAction = makeToggle(tr("&Numery wierszy"), "actionLineNumbers",
                                     m_editorSettings.lineNumbers,
                                     [](EditorSettings &s, bool on) { s.lineNumbers = on; });
    m_whitespaceAction = makeToggle(tr("Pokaż &białe znaki"), "actionShowWhitespace",
                                    m_editorSettings.showWhitespace,
                                    [](EditorSettings &s, bool on) { s.showWhitespace = on; });
    m_lineEndingsAction = makeToggle(tr("Pokaż &końce linii"), "actionShowLineEndings",
                                     m_editorSettings.showLineEndings,
                                     [](EditorSettings &s, bool on) { s.showLineEndings = on; });

    viewMenu->addSeparator();
    m_zoomInAction = viewMenu->addAction(tr("Po&większ"));
    m_zoomInAction->setObjectName(QStringLiteral("actionZoomIn"));
    m_zoomInAction->setShortcuts(
        {QKeySequence(Qt::CTRL | Qt::Key_Plus), QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    connect(m_zoomInAction, &QAction::triggered, this,
            [this] { setZoom(m_editorSettings.zoom + 1); });

    m_zoomOutAction = viewMenu->addAction(tr("Po&mniejsz"));
    m_zoomOutAction->setObjectName(QStringLiteral("actionZoomOut"));
    m_zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(m_zoomOutAction, &QAction::triggered, this,
            [this] { setZoom(m_editorSettings.zoom - 1); });

    m_zoomResetAction = viewMenu->addAction(tr("&Przywróć rozmiar"));
    m_zoomResetAction->setObjectName(QStringLiteral("actionZoomReset"));
    m_zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_zoomResetAction, &QAction::triggered, this, [this] { setZoom(0); });

    m_fontAction = new QAction(tr("&Czcionka…"), this);
    m_fontAction->setObjectName(QStringLiteral("actionFont"));
    connect(m_fontAction, &QAction::triggered, this, [this] { chooseFont(); });
}

void MainWindow::newDocument()
{
    addEditorTab(m_documents.createNew(), QString(), LineEnding::LF, targetPane(-1));
}

void MainWindow::openFilesFromDialog()
{
    const QStringList paths = m_prompts->askOpenPaths(this);
    for (const QString &path : paths)
        openFile(path);
}

bool MainWindow::saveCurrent()
{
    EditorWidget *editor = currentEditor();
    return editor && saveEditor(editor, false);
}

bool MainWindow::saveCurrentAs()
{
    EditorWidget *editor = currentEditor();
    return editor && saveEditor(editor, true);
}

bool MainWindow::saveAll()
{
    for (const DocumentId id : openDocuments()) {
        EditorWidget *editor = findEditor(id);
        if (!needsSaving(editor)) // także plik usunięty z dysku (D026)
            continue;
        activateEditor(editor);
        if (!saveEditor(editor, false))
            return false;
    }
    return true;
}

bool MainWindow::closeCurrentTab()
{
    return closeEditor(currentEditor());
}

void MainWindow::goToLine()
{
    EditorWidget *editor = currentEditor();
    if (!editor)
        return;

    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    const int lineCount = std::max(editor->lines(), 1);

    const std::optional<int> chosen = m_prompts->askLineNumber(this, line + 1, lineCount);
    if (!chosen)
        return;

    const int target = std::clamp(*chosen, 1, lineCount) - 1;
    editor->setCursorPosition(target, 0);
    editor->ensureLineVisible(target);
    editor->setFocus();
}

void MainWindow::chooseLanguage(LanguageId language)
{
    EditorWidget *editor = currentEditor();
    if (!editor)
        return;
    const DocumentId id = documentIdOf(editor);
    m_documents.setLanguage(id, language);
    for (EditorWidget *view : editorsOf(id))
        view->setLanguage(language);
    updateLanguageActions();
    updateCursorStatus();
}

void MainWindow::handleMixedLineEndings(EditorWidget *editor)
{
    const DocumentId id = documentIdOf(editor);
    switch (m_prompts->askMixedLineEndings(this, displayName(id))) {
    case MixedLineEndingChoice::Keep:
        return;
    case MixedLineEndingChoice::ConvertToLf:
        editor->convertEols(QsciScintilla::EolUnix);
        for (EditorWidget *view : editorsOf(id))
            view->setEolMode(QsciScintilla::EolUnix);
        m_documents.setLineEnding(id, LineEnding::LF);
        break;
    case MixedLineEndingChoice::ConvertToCrLf:
        editor->convertEols(QsciScintilla::EolWindows);
        for (EditorWidget *view : editorsOf(id))
            view->setEolMode(QsciScintilla::EolWindows);
        m_documents.setLineEnding(id, LineEnding::CRLF);
        break;
    }
    updateCursorStatus();
}

void MainWindow::syncEditorLanguage(EditorWidget *editor)
{
    const DocumentId id = documentIdOf(editor);
    const DocumentState *document = m_documents.document(id);
    if (document) {
        for (EditorWidget *view : editorsOf(id)) {
            if (view->language() != document->language)
                view->setLanguage(document->language);
        }
    }
    updateLanguageActions();
    updateCursorStatus();
}

bool MainWindow::saveEditor(EditorWidget *editor, bool askForPath, bool suggestCopyName)
{
    const DocumentId id = documentIdOf(editor);
    const DocumentState *document = m_documents.document(id);
    if (!document)
        return false;

    const QString name = displayName(id);
    const QString previousPath = document->filePath;
    SaveOutcome outcome;
    if (askForPath || document->isUntitled()) {
        // Nowy dokument: rozszerzenie zgodne z językiem, np. „Nowy 1.html” (D025).
        QString suggested = document->filePath;
        if (document->isUntitled()) {
            const QString extension = LanguageRegistry::defaultExtension(editor->language());
            suggested = QDir::home().filePath(
                extension.isEmpty() ? name : name + QLatin1Char('.') + extension);
        } else if (suggestCopyName) {
            // „Zapisz jako…” z konfliktu: inna nazwa niż plik zmieniony przez inny program.
            const QFileInfo info(document->filePath);
            const QString base = info.completeBaseName() + tr(" (kopia)");
            suggested = info.dir().filePath(
                info.suffix().isEmpty() ? base : base + QLatin1Char('.') + info.suffix());
        }
        beginPrompt(id);
        const QString path = m_prompts->askSavePath(this, suggested);
        endPrompt(id);
        if (path.isEmpty())
            return false;
        // Po dialogu (zagnieżdżona pętla zdarzeń) korzystamy tylko z id, nie z `document`.
        outcome = m_documents.saveAs(id, path, editor->text());
    } else {
        outcome = m_documents.save(id, editor->text());
        if (outcome.failure == SaveFailure::DiskChanged) {
            // Plik zmienił się na dysku od wczytania/zapisu (D027).
            beginPrompt(id);
            const SaveConflictChoice choice =
                m_prompts->askSaveConflict(this, QDir::toNativeSeparators(previousPath));
            endPrompt(id);
            switch (choice) {
            case SaveConflictChoice::Overwrite:
                outcome = m_documents.save(id, editor->text(), true);
                break;
            case SaveConflictChoice::SaveAs:
                return saveEditor(editor, true, true);
            case SaveConflictChoice::Cancel:
                return false;
            }
        }
    }

    if (!outcome.ok()) {
        reportSaveFailure(name, outcome);
        return false;
    }

    // Stan „zapisany” dopiero po potwierdzonym commit() (D014); bufor wspólny dla klonów.
    editor->setModified(false);
    if (const DocumentState *saved = m_documents.document(id)) {
        // QSaveFile podmienia plik (nowy i-węzeł) — obserwacja od nowa (D030).
        if (saved->filePath != previousPath) {
            m_fileWatcher.unwatch(previousPath);
            m_fileWatcher.watch(saved->filePath);
        } else {
            m_fileWatcher.rewatch(saved->filePath);
        }
    }
    updateDocumentTitles(id);
    updateWindowTitle();
    // Po „Zapisz jako” z nowym rozszerzeniem język mógł się zmienić (D023).
    syncEditorLanguage(editor);
    return true;
}

bool MainWindow::needsSaving(const EditorWidget *editor) const
{
    if (!editor)
        return false;
    const DocumentState *document = m_documents.document(documentIdOf(editor));
    // Plik usunięty z dysku: treść istnieje już tylko w edytorze (D026).
    return editor->isModified() || (document && document->missingOnDisk);
}

void MainWindow::onWatchedFileChanged(const QString &path)
{
    // Raz na dokument, niezależnie od liczby widoków.
    for (const DocumentId id : openDocuments()) {
        const DocumentState *document = m_documents.document(id);
        if (document && document->filePath == path)
            handleDiskChange(findEditor(id));
    }
}

void MainWindow::beginPrompt(DocumentId id)
{
    ++m_promptDepth[id];
}

void MainWindow::endPrompt(DocumentId id)
{
    if (--m_promptDepth[id] > 0)
        return;
    m_promptDepth.remove(id);
    // Zdarzenie dysku z czasu pytania — obsługa po powrocie do głównej pętli.
    if (m_pendingDiskEvents.remove(id)) {
        QTimer::singleShot(0, this, [this, id] {
            if (EditorWidget *editor = findEditor(id))
                handleDiskChange(editor);
        });
    }
}

void MainWindow::handleDiskChange(EditorWidget *editor)
{
    if (!editor)
        return;
    const DocumentId id = documentIdOf(editor);
    // Trwa pytanie o ten dokument (zagnieżdżona pętla zdarzeń) — odłóż, nie gub.
    if (m_promptDepth.value(id) > 0) {
        m_pendingDiskEvents.insert(id);
        return;
    }
    const DocumentState *document = m_documents.document(id);
    if (!document)
        return;
    const bool wasMissing = document->missingOnDisk;
    const QString path = QDir::toNativeSeparators(document->filePath);

    // Zdarzenie już przyszło — zawsze pełne porównanie treści (D030).
    switch (m_documents.checkDisk(id, false)) {
    case DiskStatus::Unchanged:
        break;
    case DiskStatus::Missing:
        if (!wasMissing) {
            beginPrompt(id);
            m_prompts->showInfo(this, tr("Plik usunięty z dysku"),
                                tr("Plik %1 został usunięty lub przeniesiony.\n\nTreść pozostaje "
                                   "w edytorze — zapisz ją, jeśli jest potrzebna.")
                                    .arg(path));
            endPrompt(id);
        }
        break;
    case DiskStatus::Changed: {
        // Ta sama obca wersja, przy której wybrano już „Zostaw” — bez ponownego pytania.
        if (m_documents.isDiskVersionDeclined(id))
            break;
        beginPrompt(id);
        const ExternalChangeChoice choice =
            m_prompts->askExternalChange(this, displayName(id), editor->isModified());
        EditorWidget *current = findEditor(id); // dokument mógł zostać zamknięty
        if (current) {
            if (choice == ExternalChangeChoice::Reload)
                reloadEditor(current);
            else
                m_documents.declineDiskVersion(id);
        }
        endPrompt(id);
        if (!current)
            return;
        break;
    }
    }
    updateDocumentTitles(id);
    updateWindowTitle();
    updateCursorStatus();
}

void MainWindow::reloadEditor(EditorWidget *editor)
{
    const DocumentId id = documentIdOf(editor);
    const QString path = m_documents.document(id) ? m_documents.document(id)->filePath : QString();
    const DocumentController::ReloadData data = m_documents.prepareReload(id);
    if (data.outcome.status != OpenStatus::Opened) {
        reportOpenFailure(path, data.outcome);
        m_documents.checkDisk(id); // np. plik zniknął w międzyczasie → ochrona przy zamykaniu
        return;
    }

    const QList<EditorWidget *> views = editorsOf(id);
    // Pozycja kursora każdego widoku (klony) — przywracana po przeładowaniu.
    QList<int> viewLines;
    for (EditorWidget *view : views) {
        int viewLine = 0;
        int viewIndex = 0;
        view->getCursorPosition(&viewLine, &viewIndex);
        viewLines.append(viewLine);
    }
    const QString previousText = editor->text();
    const QsciScintilla::EolMode previousEol = editor->eolMode();
    const bool previousTabs = editor->indentationsUseTabs();

    const auto applyModes = [&views](QsciScintilla::EolMode eol, bool tabs) {
        for (EditorWidget *view : views) {
            view->setEolMode(eol);
            view->setIndentationsUseTabs(tabs);
        }
    };
    applyModes(data.state.lineEnding == LineEnding::CRLF ? QsciScintilla::EolWindows
                                                         : QsciScintilla::EolUnix,
               data.state.indentWithTabs);
    if (!editor->loadText(data.outcome.text)) {
        // Stan dokumentu nie został zmieniony (bez commitReload) — przywracamy bufor.
        editor->setText(previousText);
        applyModes(previousEol, previousTabs);
        m_prompts->showError(this, tr("Nie można przeładować pliku"),
                             tr("%1\n\nTreści nie udało się wczytać do edytora bez zmian.")
                                 .arg(QDir::toNativeSeparators(path)));
        return;
    }
    m_documents.commitReload(id, data);
    const int lastLine = std::max(editor->lines() - 1, 0);
    for (qsizetype i = 0; i < views.size(); ++i)
        views.at(i)->setCursorPosition(std::min(viewLines.at(i), lastLine), 0);
    m_fileWatcher.rewatch(path);
}

bool MainWindow::resolveUnsavedChanges(EditorWidget *editor)
{
    if (!editor)
        return true;
    // Aktualny stan dysku (np. plik usunięty, a zdarzenie nie dotarło) — tani test.
    const DocumentId id = documentIdOf(editor);
    m_documents.checkDisk(id);
    if (!needsSaving(editor))
        return true;

    activateEditor(editor);
    beginPrompt(id);
    const UnsavedChoice choice = m_prompts->askUnsavedChanges(this, displayName(id));
    endPrompt(id);
    switch (choice) {
    case UnsavedChoice::Save:
        return saveEditor(editor, false);
    case UnsavedChoice::Discard:
        return true;
    case UnsavedChoice::Cancel:
        return false;
    }
    return false;
}

bool MainWindow::closeEditor(EditorWidget *editor)
{
    if (!editor)
        return false;
    const DocumentId id = documentIdOf(editor);

    // Klon: zamknięcie jednego widoku nie zamyka dokumentu (D036).
    if (editorsOf(id).size() > 1) {
        removeView(editor);
        return true;
    }
    if (!resolveUnsavedChanges(editor))
        return false;

    if (const DocumentState *document = m_documents.document(id))
        m_fileWatcher.unwatch(document->filePath);
    removeView(editor);
    m_documents.close(id);
    return true;
}

EditorWidget *MainWindow::createEditor(DocumentId id, EditorWidget *shareDocumentWith)
{
    auto *editor = new EditorWidget;
    if (shareDocumentWith) {
        // Klon: wspólny bufor, historia cofania i stan zmian (D036). Tryb końców
        // linii należy w Scintilli do dokumentu, a setDocument() narzuca mu tryb
        // nowego widoku — odczyt przed i ustawienie po, żeby nie przełączyć CRLF na LF.
        const QsciScintilla::EolMode eol = shareDocumentWith->eolMode();
        editor->setEolMode(eol);
        editor->setDocument(shareDocumentWith->document());
        editor->setEolMode(eol);
    }
    editor->setTheme(ThemePalette::forId(m_theme));
    if (const DocumentState *document = m_documents.document(id)) {
        editor->setLanguage(document->language);
        // Wcięcia w stylu pliku — bez mieszania tabulatorów ze spacjami (D024).
        editor->setIndentationsUseTabs(document->indentWithTabs);
    }
    applyEditorSettings(editor);

    m_documentIds.insert(editor, id);
    // Ctrl+kółko w jednej karcie zmienia powiększenie wszystkich i zapisuje je (D029).
    connect(editor, &QsciScintillaBase::SCN_ZOOM, this, [this, editor] {
        setZoom(static_cast<int>(editor->SendScintilla(QsciScintillaBase::SCI_GETZOOM)));
    });
    connect(editor, &QsciScintilla::modificationChanged, this, [this, editor] {
        updateTabTitle(editor);
        if (editor == currentEditor())
            updateWindowTitle();
    });
    connect(editor, &QsciScintilla::cursorPositionChanged, this, [this, editor] {
        if (editor == currentEditor())
            updateCursorStatus();
    });
    connect(editor, &QsciScintilla::selectionChanged, this, [this, editor] {
        if (editor == currentEditor())
            updateCursorStatus();
    });
    return editor;
}

EditorWidget *MainWindow::addEditorTab(DocumentId id, const QString &text, LineEnding lineEnding,
                                       int pane)
{
    EditorWidget *editor = createEditor(id);
    // Nowe linie w stylu pliku; istniejące końce linii zostają bez zmian (D016).
    editor->setEolMode(lineEnding == LineEnding::CRLF ? QsciScintilla::EolWindows
                                                      : QsciScintilla::EolUnix);
    if (!editor->loadText(text)) {
        m_documentIds.remove(editor);
        editor->disconnect(this);
        delete editor;
        return nullptr;
    }
    insertIntoPane(editor, pane);
    return editor;
}

void MainWindow::insertIntoPane(EditorWidget *editor, int pane)
{
    QTabWidget *tabs = m_panes[static_cast<size_t>(pane)];
    tabs->addTab(editor, QString());
    updateTabTitle(editor);
    updatePaneVisibility();
    activateEditor(editor);
}

void MainWindow::removeView(EditorWidget *editor)
{
    const int pane = paneOf(editor);
    if (pane >= 0) {
        QTabWidget *tabs = m_panes[static_cast<size_t>(pane)];
        tabs->removeTab(tabs->indexOf(editor));
    }
    m_documentIds.remove(editor);
    // Edytor żyje do deleteLater() poza kartami — jego sygnały nie mogą już trafiać do okna.
    editor->disconnect(this);
    editor->deleteLater();
    updatePaneVisibility();
    if (m_panes[static_cast<size_t>(m_activePane)]->count() == 0)
        m_activePane = 1 - m_activePane;
    onCurrentEditorChanged();
}

void MainWindow::activateEditor(EditorWidget *editor)
{
    const int pane = paneOf(editor);
    if (pane < 0)
        return;
    m_panes[static_cast<size_t>(pane)]->setCurrentWidget(editor);
    m_activePane = pane;
    editor->setFocus();
    onCurrentEditorChanged();
}

void MainWindow::updatePaneVisibility()
{
    QTabWidget *second = m_panes[1];
    const bool show = second->count() > 0;
    if (show == second->isVisibleTo(this))
        return;
    second->setVisible(show);
    if (show) {
        const int half = std::max(m_splitter->width() / 2, 1);
        m_splitter->setSizes({half, half});
    }
}

void MainWindow::showTabMenu(int pane, const QPoint &position)
{
    QTabWidget *tabs = m_panes[static_cast<size_t>(pane)];
    const int index = tabs->tabBar()->tabAt(position);
    auto *editor = qobject_cast<EditorWidget *>(tabs->widget(index));
    if (!editor)
        return;
    activateEditor(editor);
    QMenu menu(this);
    menu.addAction(m_moveToOtherViewAction);
    menu.addAction(m_cloneToOtherViewAction);
    menu.addSeparator();
    menu.addAction(m_closeTabAction);
    menu.exec(tabs->tabBar()->mapToGlobal(position));
}

int MainWindow::targetPane(int requested) const
{
    if (requested == 0 || requested == 1)
        return requested;
    if (m_panes[static_cast<size_t>(m_activePane)]->count() > 0)
        return m_activePane;
    return 0;
}

int MainWindow::paneOf(const EditorWidget *editor) const
{
    auto *widget = const_cast<EditorWidget *>(editor);
    for (int index = 0; index < PaneCount; ++index) {
        if (m_panes[static_cast<size_t>(index)]->indexOf(widget) >= 0)
            return index;
    }
    return -1;
}

QList<EditorWidget *> MainWindow::allEditors() const
{
    QList<EditorWidget *> editors;
    for (QTabWidget *tabs : m_panes) {
        if (!tabs)
            continue;
        for (int i = 0; i < tabs->count(); ++i) {
            if (auto *editor = qobject_cast<EditorWidget *>(tabs->widget(i)))
                editors.append(editor);
        }
    }
    return editors;
}

QList<EditorWidget *> MainWindow::editorsOf(DocumentId id) const
{
    QList<EditorWidget *> views;
    for (EditorWidget *editor : allEditors()) {
        if (documentIdOf(editor) == id)
            views.append(editor);
    }
    return views;
}

EditorWidget *MainWindow::viewInPane(DocumentId id, int pane) const
{
    QTabWidget *tabs = m_panes[static_cast<size_t>(pane)];
    for (int i = 0; i < tabs->count(); ++i) {
        auto *editor = qobject_cast<EditorWidget *>(tabs->widget(i));
        if (editor && documentIdOf(editor) == id)
            return editor;
    }
    return nullptr;
}

QList<DocumentId> MainWindow::openDocuments() const
{
    QList<DocumentId> ids;
    for (EditorWidget *editor : allEditors()) {
        const DocumentId id = documentIdOf(editor);
        if (id != InvalidDocumentId && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

EditorWidget *MainWindow::findEditor(DocumentId id) const
{
    for (EditorWidget *editor : allEditors()) {
        if (documentIdOf(editor) == id)
            return editor;
    }
    return nullptr;
}

DocumentId MainWindow::documentIdOf(const EditorWidget *editor) const
{
    return m_documentIds.value(editor, InvalidDocumentId);
}

bool MainWindow::isPristineUntitled(const EditorWidget *editor) const
{
    if (!editor)
        return false;
    const DocumentState *document = m_documents.document(documentIdOf(editor));
    return document && document->isUntitled() && !editor->isModified() && editor->length() == 0;
}

QString MainWindow::displayName(DocumentId id) const
{
    const DocumentState *document = m_documents.document(id);
    if (!document)
        return QString();
    if (document->isUntitled())
        return tr("Nowy %1").arg(document->untitledNumber);
    return QFileInfo(document->filePath).fileName();
}

QString MainWindow::sessionFilePath() const
{
    if (!m_settingsStore)
        return QString();
    return QFileInfo(m_settingsStore->fileName()).absoluteDir().filePath(
        QStringLiteral("session.json"));
}

Session MainWindow::captureSession() const
{
    Session session;
    for (int p = 0; p < PaneCount; ++p) {
        QTabWidget *tabs = m_panes[static_cast<size_t>(p)];
        SessionPane &pane = session.panes[static_cast<size_t>(p)];
        for (int i = 0; i < tabs->count(); ++i) {
            auto *editor = qobject_cast<EditorWidget *>(tabs->widget(i));
            const DocumentState *document = m_documents.document(documentIdOf(editor));
            if (!document || document->isUntitled())
                continue; // bez treści w sesji (D034, D035)
            SessionTab tab;
            tab.path = document->filePath;
            editor->getCursorPosition(&tab.line, &tab.index);
            tab.firstVisibleLine = editor->firstVisibleLine();
            if (document->languageChosenManually)
                tab.languageKey = LanguageRegistry::key(document->language);
            if (i == tabs->currentIndex())
                pane.current = static_cast<int>(pane.tabs.size());
            pane.tabs.append(tab);
        }
    }
    session.activePane = m_activePane;
    if (!m_fileTree->isHidden())
        session.workspaceFolder = workspaceFolder();
    if (!m_panes[1]->isHidden())
        session.splitterSizes = m_splitter->sizes();
    return session;
}

bool MainWindow::saveSession() const
{
    // Przy wyłączonym przywracaniu nie zapamiętujemy ścieżek plików (D034).
    const QString path = sessionFilePath();
    return !path.isEmpty() && m_editorSettings.restoreSession
           && SessionService::save(path, captureSession());
}

bool MainWindow::restoreSession()
{
    if (!m_settingsService || !m_editorSettings.restoreSession)
        return false;
    const std::optional<Session> session = SessionService::load(sessionFilePath());
    if (!session)
        return false;

    m_restoringSession = true;
    QStringList missing;
    for (int p = 0; p < PaneCount; ++p) {
        const SessionPane &pane = session->panes[static_cast<size_t>(p)];
        for (const SessionTab &tab : pane.tabs) {
            if (!QFileInfo(tab.path).isFile()) {
                missing.append(QDir::toNativeSeparators(tab.path));
                continue;
            }
            EditorWidget *view = nullptr;
            if (const std::optional<DocumentId> open = m_documents.findOpen(tab.path)) {
                // Ten sam plik w drugim panelu = klon (D036).
                view = viewInPane(*open, p);
                if (!view) {
                    if (EditorWidget *source = findEditor(*open)) {
                        view = createEditor(*open, source);
                        insertIntoPane(view, p);
                    }
                }
            } else if (openFile(tab.path, p)) {
                if (const std::optional<DocumentId> opened = m_documents.findOpen(tab.path))
                    view = viewInPane(*opened, p);
            }
            if (!view)
                continue;

            const DocumentId id = documentIdOf(view);
            if (const std::optional<LanguageId> language =
                    LanguageRegistry::fromKey(tab.languageKey)) {
                m_documents.setLanguage(id, *language);
                for (EditorWidget *other : editorsOf(id))
                    other->setLanguage(*language);
            }
            // Wartości z pliku sesji przycinamy do treści (plik mógł się zmienić lub
            // zostać uszkodzony — ogromna kolumna zawiesiłaby start).
            const int lastLine = std::max(view->lines() - 1, 0);
            const int line = std::min(tab.line, lastLine);
            const int index =
                std::min<int>(tab.index, static_cast<int>(view->text(line).size()));
            view->setCursorPosition(line, index);
            view->setFirstVisibleLine(std::min(tab.firstVisibleLine, lastLine));
        }
    }

    // Aktywne karty w panelach i aktywny panel.
    for (int p = 0; p < PaneCount; ++p) {
        const SessionPane &pane = session->panes[static_cast<size_t>(p)];
        if (pane.current < 0 || pane.current >= pane.tabs.size())
            continue;
        if (const std::optional<DocumentId> id =
                m_documents.findOpen(pane.tabs.at(pane.current).path)) {
            if (EditorWidget *view = viewInPane(*id, p))
                m_panes[static_cast<size_t>(p)]->setCurrentWidget(view);
        }
    }
    const int active = session->activePane;
    if (EditorWidget *editor =
            qobject_cast<EditorWidget *>(m_panes[static_cast<size_t>(active)]->currentWidget()))
        activateEditor(editor);
    if (session->splitterSizes.size() == PaneCount && !m_panes[1]->isHidden())
        m_splitter->setSizes(session->splitterSizes);
    openFolder(session->workspaceFolder);
    m_restoringSession = false;

    if (!missing.isEmpty()) {
        m_prompts->showInfo(this, tr("Przywracanie sesji"),
                            tr("Nie znaleziono plików z poprzedniej sesji (pominięto):\n\n%1")
                                .arg(missing.join(QLatin1Char('\n'))));
    }
    onCurrentEditorChanged();
    return true;
}

void MainWindow::onCurrentEditorChanged()
{
    updateWindowTitle();
    updateActions();
    updateCursorStatus();
    updateLanguageActions();
}

void MainWindow::updateTabTitle(EditorWidget *editor)
{
    const int pane = paneOf(editor);
    if (pane < 0)
        return;
    QTabWidget *tabs = m_panes[static_cast<size_t>(pane)];
    const int index = tabs->indexOf(editor);

    const DocumentId id = documentIdOf(editor);
    const DocumentState *document = m_documents.document(id);
    const QString name = displayName(id);

    QString title = escapeMnemonic(name);
    if (needsSaving(editor))
        title += QLatin1Char('*');
    tabs->setTabText(index, title);
    tabs->setTabToolTip(index, document && !document->isUntitled()
                                   ? QDir::toNativeSeparators(document->filePath)
                                   : name);
}

void MainWindow::updateDocumentTitles(DocumentId id)
{
    for (EditorWidget *view : editorsOf(id))
        updateTabTitle(view);
}

void MainWindow::updateWindowTitle()
{
    EditorWidget *editor = currentEditor();
    if (!editor) {
        setWindowTitle(AppInfo::productName());
        setWindowModified(false);
        return;
    }
    setWindowTitle(
        QStringLiteral("%1[*] — %2").arg(displayName(documentIdOf(editor)), AppInfo::productName()));
    setWindowModified(editor->isModified());
}

void MainWindow::updateActions()
{
    if (!m_saveAction)
        return; // akcje jeszcze nie utworzone (wczesne sygnały paneli)
    const bool hasEditor = currentEditor() != nullptr;
    for (QAction *action : {m_saveAction, m_saveAsAction, m_closeTabAction, m_undoAction,
                            m_redoAction, m_cutAction, m_copyAction, m_pasteAction,
                            m_selectAllAction, m_goToLineAction, m_findAction, m_replaceAction,
                            m_findNextAction, m_findPreviousAction, m_moveToOtherViewAction,
                            m_cloneToOtherViewAction}) {
        action->setEnabled(hasEditor);
    }
    m_saveAllAction->setEnabled(documentCount() > 0);
    m_findInAllAction->setEnabled(documentCount() > 0);
    m_languageGroup->setEnabled(hasEditor);
}

void MainWindow::updateLanguageActions()
{
    EditorWidget *editor = currentEditor();
    for (QAction *action : std::as_const(m_languageActions)) {
        action->setChecked(editor
                           && action->data().toInt() == static_cast<int>(editor->language()));
    }
}

void MainWindow::updateCursorStatus()
{
    if (!m_cursorLabel)
        return;
    EditorWidget *editor = currentEditor();
    const DocumentState *document = editor ? m_documents.document(documentIdOf(editor)) : nullptr;
    if (!editor || !document) {
        for (QLabel *label : {m_cursorLabel, m_languageLabel, m_encodingLabel, m_lineEndingLabel})
            label->clear();
        return;
    }

    // Kolumna w znakach (nie bajtach), tabulator rozwinięty do szerokości tabulacji.
    const long caret = editor->SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
    const long line = editor->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION,
                                            static_cast<unsigned long>(caret));
    const long column =
        editor->SendScintilla(QsciScintillaBase::SCI_GETCOLUMN, static_cast<unsigned long>(caret));
    const long selected = editor->SendScintilla(
        QsciScintillaBase::SCI_COUNTCHARACTERS,
        static_cast<unsigned long>(editor->SendScintilla(QsciScintillaBase::SCI_GETSELECTIONSTART)),
        editor->SendScintilla(QsciScintillaBase::SCI_GETSELECTIONEND));
    QString position = tr("Wiersz %1, kolumna %2").arg(line + 1).arg(column + 1);
    if (selected > 0)
        position += tr(", zaznaczono %1").arg(selected);
    m_cursorLabel->setText(position);

    m_languageLabel->setText(LanguageRegistry::displayName(editor->language()));
    m_encodingLabel->setText(document->hasBom ? tr("UTF-8 z BOM") : QStringLiteral("UTF-8"));
    switch (document->lineEnding) {
    case LineEnding::CRLF:
        m_lineEndingLabel->setText(QStringLiteral("CRLF"));
        break;
    case LineEnding::Mixed:
        m_lineEndingLabel->setText(tr("Mieszane"));
        break;
    case LineEnding::LF:
        m_lineEndingLabel->setText(QStringLiteral("LF"));
        break;
    case LineEnding::None:
        // Brak końców linii w pliku — nowe linie wpisywane wg trybu edytora.
        m_lineEndingLabel->setText(editor->eolMode() == QsciScintilla::EolWindows
                                       ? QStringLiteral("CRLF")
                                       : QStringLiteral("LF"));
        break;
    }
}

void MainWindow::reportOpenFailure(const QString &path, const OpenOutcome &outcome)
{
    QString reason;
    switch (outcome.failure) {
    case OpenFailure::Binary:
        reason = tr("Plik zawiera bajty NUL i wygląda na binarny. "
                    "Otwieranie anulowano, aby nie uszkodzić danych.");
        break;
    case OpenFailure::InvalidUtf8:
        reason = tr("Plik nie jest poprawnym tekstem UTF-8. Ta wersja CynamonCode otwiera "
                    "tylko pliki UTF-8; plik pozostał bez zmian.");
        break;
    case OpenFailure::File:
    case OpenFailure::None:
        reason = fileErrorText(outcome.fileError, outcome.systemMessage);
        break;
    }
    m_prompts->showError(this, tr("Nie można otworzyć pliku"),
                         QStringLiteral("%1\n\n%2").arg(QDir::toNativeSeparators(path), reason));
}

void MainWindow::reportSaveFailure(const QString &name, const SaveOutcome &outcome)
{
    QString reason;
    switch (outcome.failure) {
    case SaveFailure::DiskChanged:
        reason = tr("Plik na dysku zmienił się od ostatniego wczytania lub zapisu.");
        break;
    case SaveFailure::OpenInAnotherDocument:
        reason = tr("Ten plik jest już otwarty w innej karcie. "
                    "Zamknij tamtą kartę albo wybierz inną nazwę.");
        break;
    case SaveFailure::File:
        reason = fileErrorText(outcome.fileError, outcome.systemMessage);
        break;
    case SaveFailure::NoPath:
    case SaveFailure::UnknownDocument:
    case SaveFailure::None:
        reason = tr("Dokument nie ma przypisanego pliku.");
        break;
    }
    m_prompts->showError(this, tr("Nie można zapisać dokumentu"),
                         tr("Nie zapisano dokumentu „%1”.\n\n%2\n\nZmiany pozostają w edytorze.")
                             .arg(name, reason));
}

QString MainWindow::fileErrorText(FileError error, const QString &systemMessage)
{
    switch (error) {
    case FileError::None:
        return QString();
    case FileError::NotFound:
        return tr("Plik nie istnieje.");
    case FileError::NotRegularFile:
        return tr("Wskazana ścieżka nie jest zwykłym plikiem.");
    case FileError::NotReadable:
        return tr("Brak uprawnień do odczytu pliku.");
    case FileError::NotWritable:
        return tr("Plik jest tylko do odczytu. Oryginał pozostał bez zmian.");
    case FileError::BrokenSymlink:
        return tr("Dowiązanie symboliczne wskazuje na nieistniejący plik.");
    case FileError::OpenFailed:
        return tr("Operacja na pliku nie powiodła się: %1. Oryginał pozostał bez zmian.")
            .arg(systemMessage);
    case FileError::ReadFailed:
        return tr("Błąd odczytu: %1").arg(systemMessage);
    case FileError::WriteFailed:
        return tr("Błąd zapisu: %1. Oryginał pozostał bez zmian.").arg(systemMessage);
    case FileError::CommitFailed:
        return tr("Nie udało się zakończyć zapisu: %1. Oryginał pozostał bez zmian.")
            .arg(systemMessage);
    }
    return QString();
}

} // namespace cynamoncode
