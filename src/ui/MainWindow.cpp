#include "ui/MainWindow.h"

#include "app/AppInfo.h"
#include "editor/EditorWidget.h"
#include "services/LanguageRegistry.h"
#include "services/SearchService.h"
#include "ui/AboutDialog.h"
#include "ui/FindReplaceDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSettings>
#include <QTimer>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>

#include <algorithm>
#include <utility>

namespace cynamoncode {

namespace {

// QTabBar traktuje „&” jako skrót klawiszowy — w nazwach plików trzeba go podwoić.
QString escapeMnemonic(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

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

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("documentTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

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

    createActions();
    createMenus();
    createViewAndLanguageMenus();

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
    connect(&m_fileWatcher, &FileWatchService::fileChanged, this,
            &MainWindow::onWatchedFileChanged);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        updateWindowTitle();
        updateActions();
        updateCursorStatus();
        updateLanguageActions();
    });

    newDocument();
}

MainWindow::~MainWindow()
{
    // Sloty korzystają z pól tej klasy — odłącz je, zanim Qt zniszczy karty.
    m_tabs->disconnect(this);
    for (int i = 0; i < m_tabs->count(); ++i)
        m_tabs->widget(i)->disconnect(this);
    delete m_tabs;
    m_tabs = nullptr;
}

bool MainWindow::openFile(const QString &path)
{
    EditorWidget *pristine =
        (m_tabs->count() == 1 && isPristineUntitled(editorAt(0))) ? editorAt(0) : nullptr;

    const OpenOutcome outcome = m_documents.open(path);
    switch (outcome.status) {
    case OpenStatus::AlreadyOpen:
        if (EditorWidget *existing = findEditor(outcome.id)) {
            m_tabs->setCurrentWidget(existing);
            existing->setFocus();
        }
        return true;
    case OpenStatus::Failed:
        reportOpenFailure(path, outcome);
        return false;
    case OpenStatus::Opened:
        break;
    }

    const DocumentState *document = m_documents.document(outcome.id);
    EditorWidget *editor = addEditorTab(outcome.id, outcome.text, document->lineEnding);
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
        closeTab(m_tabs->indexOf(pristine));

    if (document->lineEnding == LineEnding::Mixed)
        handleMixedLineEndings(editor);
    return true;
}

void MainWindow::setTheme(ThemeId theme)
{
    m_theme = theme;
    const ThemePalette palette = ThemePalette::forId(theme);
    for (int i = 0; i < m_tabs->count(); ++i)
        editorAt(i)->setTheme(palette);
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
    for (int i = 0; i < m_tabs->count(); ++i)
        applyEditorSettings(editorAt(i));
    if (m_settingsService)
        m_settingsService->save(m_editorSettings);

    if (m_wordWrapAction) {
        m_wordWrapAction->setChecked(m_editorSettings.wordWrap);
        m_lineNumbersAction->setChecked(m_editorSettings.lineNumbers);
        m_whitespaceAction->setChecked(m_editorSettings.showWhitespace);
        m_lineEndingsAction->setChecked(m_editorSettings.showLineEndings);
    }
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
    return m_tabs->count();
}

EditorWidget *MainWindow::currentEditor() const
{
    return qobject_cast<EditorWidget *>(m_tabs->currentWidget());
}

EditorWidget *MainWindow::editorAt(int index) const
{
    return qobject_cast<EditorWidget *>(m_tabs->widget(index));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (!resolveUnsavedChanges(editorAt(i))) {
            event->ignore();
            return;
        }
    }
    if (m_settingsService)
        m_settingsService->saveWindowGeometry(saveGeometry());
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
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Plik"));
    fileMenu->setObjectName(QStringLiteral("menuFile"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
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

void MainWindow::reportSearchStatus(const QString &message)
{
    if (m_findDialog)
        m_findDialog->setStatus(message);
    if (message.isEmpty())
        statusBar()->clearMessage();
    else
        statusBar()->showMessage(message, 5000);
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
    addEditorTab(m_documents.createNew(), QString(), LineEnding::LF);
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
    for (int i = 0; i < m_tabs->count(); ++i) {
        EditorWidget *editor = editorAt(i);
        if (!needsSaving(editor)) // także plik usunięty z dysku (D026)
            continue;
        m_tabs->setCurrentIndex(i);
        if (!saveEditor(editor, false))
            return false;
    }
    return true;
}

bool MainWindow::closeCurrentTab()
{
    const int index = m_tabs->currentIndex();
    return index >= 0 && closeTab(index);
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
    m_documents.setLanguage(documentIdOf(editor), language);
    editor->setLanguage(language);
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
        editor->setEolMode(QsciScintilla::EolUnix);
        m_documents.setLineEnding(id, LineEnding::LF);
        break;
    case MixedLineEndingChoice::ConvertToCrLf:
        editor->convertEols(QsciScintilla::EolWindows);
        editor->setEolMode(QsciScintilla::EolWindows);
        m_documents.setLineEnding(id, LineEnding::CRLF);
        break;
    }
    updateCursorStatus();
}

void MainWindow::syncEditorLanguage(EditorWidget *editor)
{
    const DocumentState *document = m_documents.document(documentIdOf(editor));
    if (document && editor->language() != document->language)
        editor->setLanguage(document->language);
    if (editor == currentEditor()) {
        updateLanguageActions();
        updateCursorStatus();
    }
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

    // Stan „zapisany” dopiero po potwierdzonym commit() (D014).
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
    updateTabTitle(editor);
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
    for (int i = 0; i < m_tabs->count(); ++i) {
        EditorWidget *editor = editorAt(i);
        const DocumentState *document = m_documents.document(documentIdOf(editor));
        if (document && document->filePath == path)
            handleDiskChange(editor);
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
        const bool stillOpen = findEditor(id) == editor;
        if (stillOpen) {
            if (choice == ExternalChangeChoice::Reload)
                reloadEditor(editor);
            else
                m_documents.declineDiskVersion(id);
        }
        endPrompt(id);
        if (!stillOpen)
            return;
        break;
    }
    }
    updateTabTitle(editor);
    if (editor == currentEditor()) {
        updateWindowTitle();
        updateCursorStatus();
    }
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

    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    const QString previousText = editor->text();
    const QsciScintilla::EolMode previousEol = editor->eolMode();
    const bool previousTabs = editor->indentationsUseTabs();

    editor->setEolMode(data.state.lineEnding == LineEnding::CRLF ? QsciScintilla::EolWindows
                                                                 : QsciScintilla::EolUnix);
    editor->setIndentationsUseTabs(data.state.indentWithTabs);
    if (!editor->loadText(data.outcome.text)) {
        // Stan dokumentu nie został zmieniony (bez commitReload) — przywracamy bufor.
        editor->setText(previousText);
        editor->setEolMode(previousEol);
        editor->setIndentationsUseTabs(previousTabs);
        m_prompts->showError(this, tr("Nie można przeładować pliku"),
                             tr("%1\n\nTreści nie udało się wczytać do edytora bez zmian.")
                                 .arg(QDir::toNativeSeparators(path)));
        return;
    }
    m_documents.commitReload(id, data);
    const int lastLine = std::max(editor->lines() - 1, 0);
    editor->setCursorPosition(std::min(line, lastLine), 0);
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

    m_tabs->setCurrentWidget(editor);
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

bool MainWindow::closeTab(int index)
{
    EditorWidget *editor = editorAt(index);
    if (!editor || !resolveUnsavedChanges(editor))
        return false;

    const DocumentId id = documentIdOf(editor);
    if (const DocumentState *document = m_documents.document(id))
        m_fileWatcher.unwatch(document->filePath);
    m_tabs->removeTab(m_tabs->indexOf(editor));
    m_documentIds.remove(editor);
    m_documents.close(id);
    // Edytor żyje do deleteLater() poza kartami — jego sygnały nie mogą już trafiać do okna.
    editor->disconnect(this);
    editor->deleteLater();

    updateWindowTitle();
    updateActions();
    updateCursorStatus();
    return true;
}

EditorWidget *MainWindow::addEditorTab(DocumentId id, const QString &text, LineEnding lineEnding)
{
    auto *editor = new EditorWidget(m_tabs);
    editor->setTheme(ThemePalette::forId(m_theme));
    if (const DocumentState *document = m_documents.document(id)) {
        editor->setLanguage(document->language);
        // Wcięcia w stylu pliku — bez mieszania tabulatorów ze spacjami (D024).
        editor->setIndentationsUseTabs(document->indentWithTabs);
    }
    applyEditorSettings(editor);
    // Ctrl+kółko w jednej karcie zmienia powiększenie wszystkich i zapisuje je (D029).
    connect(editor, &QsciScintillaBase::SCN_ZOOM, this, [this, editor] {
        setZoom(static_cast<int>(editor->SendScintilla(QsciScintillaBase::SCI_GETZOOM)));
    });
    // Nowe linie w stylu pliku; istniejące końce linii zostają bez zmian (D016).
    editor->setEolMode(lineEnding == LineEnding::CRLF ? QsciScintilla::EolWindows
                                                      : QsciScintilla::EolUnix);
    if (!editor->loadText(text)) {
        delete editor;
        return nullptr;
    }

    m_documentIds.insert(editor, id);
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

    const int index = m_tabs->addTab(editor, QString());
    updateTabTitle(editor);
    m_tabs->setCurrentIndex(index);
    editor->setFocus();

    updateWindowTitle();
    updateActions();
    updateCursorStatus();
    return editor;
}

EditorWidget *MainWindow::findEditor(DocumentId id) const
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        EditorWidget *editor = editorAt(i);
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

void MainWindow::updateTabTitle(EditorWidget *editor)
{
    const int index = m_tabs->indexOf(editor);
    if (index < 0)
        return;

    const DocumentId id = documentIdOf(editor);
    const DocumentState *document = m_documents.document(id);
    const QString name = displayName(id);

    QString title = escapeMnemonic(name);
    if (needsSaving(editor))
        title += QLatin1Char('*');
    m_tabs->setTabText(index, title);
    m_tabs->setTabToolTip(index, document && !document->isUntitled()
                                     ? QDir::toNativeSeparators(document->filePath)
                                     : name);
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
    const bool hasEditor = currentEditor() != nullptr;
    for (QAction *action : {m_saveAction, m_saveAsAction, m_closeTabAction, m_undoAction,
                            m_redoAction, m_cutAction, m_copyAction, m_pasteAction,
                            m_selectAllAction, m_goToLineAction, m_findAction, m_replaceAction,
                            m_findNextAction, m_findPreviousAction}) {
        action->setEnabled(hasEditor);
    }
    m_saveAllAction->setEnabled(m_tabs->count() > 0);
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
