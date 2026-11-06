#include "ui/MainWindow.h"

#include "editor/EditorWidget.h"
#include "services/LanguageRegistry.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileInfo>
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

// Nazwa produktu nie podlega tłumaczeniu.
QString productName()
{
    return QStringLiteral("CynamonCode");
}

// QTabBar traktuje „&” jako skrót klawiszowy — w nazwach plików trzeba go podwoić.
QString escapeMnemonic(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

} // namespace

MainWindow::MainWindow(std::unique_ptr<UserPrompts> prompts, QWidget *parent)
    : QMainWindow(parent)
    , m_prompts(std::move(prompts))
{
    if (!m_prompts)
        m_prompts = std::make_unique<DialogPrompts>();

    setObjectName(QStringLiteral("MainWindow"));
    resize(1024, 720);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("documentTabs"));
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

    m_cursorLabel = new QLabel(this);
    m_cursorLabel->setObjectName(QStringLiteral("cursorLabel"));
    statusBar()->addPermanentWidget(m_cursorLabel);

    createActions();
    createMenus();
    createViewAndLanguageMenus();

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
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
    editMenu->addSeparator();
    editMenu->addAction(m_goToLineAction);
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
    m_lightThemeAction->setChecked(true);
    connect(m_lightThemeAction, &QAction::triggered, this, [this] { setTheme(ThemeId::Light); });
    connect(m_darkThemeAction, &QAction::triggered, this, [this] { setTheme(ThemeId::Dark); });

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
        if (!editor->isModified())
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
        return;
    case MixedLineEndingChoice::ConvertToCrLf:
        editor->convertEols(QsciScintilla::EolWindows);
        editor->setEolMode(QsciScintilla::EolWindows);
        m_documents.setLineEnding(id, LineEnding::CRLF);
        return;
    }
}

void MainWindow::syncEditorLanguage(EditorWidget *editor)
{
    const DocumentState *document = m_documents.document(documentIdOf(editor));
    if (document && editor->language() != document->language)
        editor->setLanguage(document->language);
    if (editor == currentEditor())
        updateLanguageActions();
}

bool MainWindow::saveEditor(EditorWidget *editor, bool askForPath)
{
    const DocumentId id = documentIdOf(editor);
    const DocumentState *document = m_documents.document(id);
    if (!document)
        return false;

    const QString name = displayName(id);
    SaveOutcome outcome;
    if (askForPath || document->isUntitled()) {
        // Nowy dokument: rozszerzenie zgodne z językiem, np. „Nowy 1.html” (D025).
        QString suggested = document->filePath;
        if (document->isUntitled()) {
            const QString extension = LanguageRegistry::defaultExtension(editor->language());
            suggested = QDir::home().filePath(
                extension.isEmpty() ? name : name + QLatin1Char('.') + extension);
        }
        const QString path = m_prompts->askSavePath(this, suggested);
        if (path.isEmpty())
            return false;
        // Po dialogu (zagnieżdżona pętla zdarzeń) korzystamy tylko z id, nie z `document`.
        outcome = m_documents.saveAs(id, path, editor->text());
    } else {
        outcome = m_documents.save(id, editor->text());
    }

    if (!outcome.ok()) {
        reportSaveFailure(name, outcome);
        return false;
    }

    // Stan „zapisany” dopiero po potwierdzonym commit() (D014).
    editor->setModified(false);
    updateTabTitle(editor);
    updateWindowTitle();
    // Po „Zapisz jako” z nowym rozszerzeniem język mógł się zmienić (D023).
    syncEditorLanguage(editor);
    return true;
}

bool MainWindow::resolveUnsavedChanges(EditorWidget *editor)
{
    if (!editor || !editor->isModified())
        return true;

    m_tabs->setCurrentWidget(editor);
    switch (m_prompts->askUnsavedChanges(this, displayName(documentIdOf(editor)))) {
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
    if (editor->isModified())
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
        setWindowTitle(productName());
        setWindowModified(false);
        return;
    }
    setWindowTitle(
        QStringLiteral("%1[*] — %2").arg(displayName(documentIdOf(editor)), productName()));
    setWindowModified(editor->isModified());
}

void MainWindow::updateActions()
{
    const bool hasEditor = currentEditor() != nullptr;
    for (QAction *action : {m_saveAction, m_saveAsAction, m_closeTabAction, m_undoAction,
                            m_redoAction, m_cutAction, m_copyAction, m_pasteAction,
                            m_selectAllAction, m_goToLineAction}) {
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
    if (!editor) {
        m_cursorLabel->clear();
        return;
    }
    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    m_cursorLabel->setText(tr("Wiersz %1, kolumna %2").arg(line + 1).arg(index + 1));
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
