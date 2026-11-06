#pragma once

#include "controllers/DocumentController.h"
#include "domain/DocumentState.h"
#include "domain/SearchOptions.h"
#include "editor/ThemePalette.h"
#include "services/FileWatchService.h"
#include "services/SessionService.h"
#include "services/SettingsService.h"
#include "ui/UserPrompts.h"

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QSet>
#include <QSettings> // pełny typ: domyślny argument std::unique_ptr<QSettings>

#include <array>
#include <functional>
#include <memory>

class QAction;
class QActionGroup;
class QCloseEvent;
class QLabel;
class QSplitter;
class QTabWidget;

namespace cynamoncode {

class AboutDialog;
class EditorWidget;
class FileTreeDock;
class FindReplaceDialog;
class SearchResultsDock;

// Główne okno: dwa panele kart (D036), panel plików, wyniki wyszukiwania, menu
// i pasek stanu. Operacje na plikach deleguje do DocumentController; nie czyta
// bajtów ani nie dekoduje tekstu. Jeden dokument może mieć kilka widoków
// (klon ze wspólnym buforem QScintilla) — operacje dotyczą dokumentu, nie karty.
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    static constexpr int PaneCount = 2;

    // Bez `settings` ustawienia i sesja działają tylko w pamięci (testy) — nic
    // nie trafia na dysk (D029, D034).
    explicit MainWindow(std::unique_ptr<UserPrompts> prompts = {},
                        std::unique_ptr<QSettings> settings = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Otwiera plik w nowej karcie (w panelu `pane`, domyślnie aktywnym) albo
    // aktywuje widok, w którym już jest otwarty.
    bool openFile(const QString &path, int pane = -1);

    // Liczba kart we wszystkich panelach; editorAt() liczy panel 0, potem 1.
    int documentCount() const;
    EditorWidget *currentEditor() const;
    EditorWidget *editorAt(int index) const;
    QTabWidget *pane(int index) const { return m_panes.at(static_cast<size_t>(index)); }
    int activePane() const { return m_activePane; }

    // Podział widoku (D036).
    bool moveCurrentToOtherView();
    bool cloneCurrentToOtherView();

    // Motyw dla wszystkich kart.
    void setTheme(ThemeId theme);
    ThemeId theme() const { return m_theme; }

    const EditorSettings &editorSettings() const { return m_editorSettings; }
    void setZoom(int zoom);

    // Wyszukiwanie w bieżącej karcie (D028); wynik także w oknie i pasku stanu.
    bool findInCurrent(const SearchOptions &options, bool forward);
    bool replaceInCurrent(const SearchOptions &options, const QString &replacement);
    int replaceAllInCurrent(const SearchOptions &options, const QString &replacement);
    FindReplaceDialog *findDialog();

    // Wszystkie otwarte dokumenty (D038); zwraca liczbę trafień / zamian.
    int findInAllDocuments(const SearchOptions &options);
    int replaceInAllDocuments(const SearchOptions &options, const QString &replacement);
    SearchResultsDock *searchResults() const { return m_searchResults; }

    // Panel plików (D037).
    void openFolder(const QString &folder);
    QString workspaceFolder() const;
    FileTreeDock *fileTree() const { return m_fileTree; }

    // Sesja (D034): tylko z plikiem ustawień; restoreSession respektuje ustawienie.
    QString sessionFilePath() const;
    bool restoreSession();
    bool saveSession() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createPanes();
    void createActions();
    void createMenus();
    void createViewAndLanguageMenus();
    void createSearchActions();
    void createSettingsActions(QMenu *viewMenu);
    void applyEditorSettings(EditorWidget *editor) const;
    void changeSettings(const std::function<void(EditorSettings &)> &change);
    QFont editorFont() const;
    void chooseFont();
    void openFindDialog(bool replace);
    void findFromMenu(bool forward);
    void findInAllFromMenu();
    void reportSearchStatus(const QString &message);
    void showSearchHit(DocumentId id, qsizetype start, qsizetype length);
    void openFolderFromDialog();

    void newDocument();
    void openFilesFromDialog();
    bool saveCurrent();
    bool saveCurrentAs();
    bool saveAll();
    bool closeCurrentTab();
    void goToLine();
    void chooseLanguage(LanguageId language);
    void handleMixedLineEndings(EditorWidget *editor);
    void syncEditorLanguage(EditorWidget *editor);

    bool saveEditor(EditorWidget *editor, bool askForPath, bool suggestCopyName = false);
    void beginPrompt(DocumentId id);
    void endPrompt(DocumentId id);
    bool needsSaving(const EditorWidget *editor) const;
    bool resolveUnsavedChanges(EditorWidget *editor);
    void onWatchedFileChanged(const QString &path);
    void handleDiskChange(EditorWidget *editor);
    void reloadEditor(EditorWidget *editor);
    bool closeEditor(EditorWidget *editor);

    // Widoki i panele.
    EditorWidget *createEditor(DocumentId id, EditorWidget *shareDocumentWith = nullptr);
    EditorWidget *addEditorTab(DocumentId id, const QString &text, LineEnding lineEnding,
                               int pane);
    void insertIntoPane(EditorWidget *editor, int pane);
    void removeView(EditorWidget *editor);
    void activateEditor(EditorWidget *editor);
    void updatePaneVisibility();
    void showTabMenu(int pane, const QPoint &position);
    int targetPane(int requested) const;
    int paneOf(const EditorWidget *editor) const;
    QList<EditorWidget *> allEditors() const;
    QList<EditorWidget *> editorsOf(DocumentId id) const;
    EditorWidget *viewInPane(DocumentId id, int pane) const;
    QList<DocumentId> openDocuments() const;
    EditorWidget *findEditor(DocumentId id) const;
    DocumentId documentIdOf(const EditorWidget *editor) const;
    bool isPristineUntitled(const EditorWidget *editor) const;
    QString displayName(DocumentId id) const;
    Session captureSession() const;

    void onCurrentEditorChanged();
    void updateTabTitle(EditorWidget *editor);
    void updateDocumentTitles(DocumentId id);
    void updateWindowTitle();
    void updateActions();
    void updateCursorStatus();
    void updateLanguageActions();

    void reportOpenFailure(const QString &path, const OpenOutcome &outcome);
    void reportSaveFailure(const QString &name, const SaveOutcome &outcome);
    static QString fileErrorText(FileError error, const QString &systemMessage);

    std::unique_ptr<UserPrompts> m_prompts;
    std::unique_ptr<QSettings> m_settingsStore;
    std::unique_ptr<SettingsService> m_settingsService;
    EditorSettings m_editorSettings;
    DocumentController m_documents;
    QHash<const EditorWidget *, DocumentId> m_documentIds;
    FileWatchService m_fileWatcher;
    // Otwarte pytania dotyczące dokumentu (zagnieżdżone pętle zdarzeń dialogów)
    // i zdarzenia dysku odłożone do ich zamknięcia.
    QHash<DocumentId, int> m_promptDepth;
    QSet<DocumentId> m_pendingDiskEvents;
    bool m_restoringSession = false;

    QSplitter *m_splitter = nullptr;
    std::array<QTabWidget *, PaneCount> m_panes{};
    int m_activePane = 0;
    FileTreeDock *m_fileTree = nullptr;
    SearchResultsDock *m_searchResults = nullptr;

    QLabel *m_cursorLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_encodingLabel = nullptr;
    QLabel *m_lineEndingLabel = nullptr;

    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_openFolderAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_saveAllAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_selectAllAction = nullptr;
    QAction *m_goToLineAction = nullptr;
    QAction *m_findAction = nullptr;
    QAction *m_replaceAction = nullptr;
    QAction *m_findNextAction = nullptr;
    QAction *m_findPreviousAction = nullptr;
    QAction *m_findInAllAction = nullptr;
    QAction *m_moveToOtherViewAction = nullptr;
    QAction *m_cloneToOtherViewAction = nullptr;
    FindReplaceDialog *m_findDialog = nullptr;
    AboutDialog *m_aboutDialog = nullptr;
    QAction *m_wordWrapAction = nullptr;
    QAction *m_lineNumbersAction = nullptr;
    QAction *m_whitespaceAction = nullptr;
    QAction *m_lineEndingsAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    QAction *m_fontAction = nullptr;
    QAction *m_restoreSessionAction = nullptr;
    QAction *m_lightThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QActionGroup *m_languageGroup = nullptr;
    QList<QAction *> m_languageActions; // QAction::data() = int(LanguageId)

    ThemeId m_theme = ThemeId::Light;
};

} // namespace cynamoncode
