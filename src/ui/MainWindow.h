#pragma once

#include "controllers/DocumentController.h"
#include "domain/DocumentState.h"
#include "editor/ThemePalette.h"
#include "ui/UserPrompts.h"

#include <QHash>
#include <QList>
#include <QMainWindow>

#include <memory>

class QAction;
class QActionGroup;
class QCloseEvent;
class QLabel;
class QTabWidget;

namespace cynamoncode {

class EditorWidget;

// Główne okno: karty edytorów, menu i pasek stanu. Operacje na plikach
// deleguje do DocumentController; nie czyta bajtów ani nie dekoduje tekstu.
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(std::unique_ptr<UserPrompts> prompts = {}, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Otwiera plik w nowej karcie albo aktywuje kartę, w której już jest otwarty.
    bool openFile(const QString &path);

    int documentCount() const;
    EditorWidget *currentEditor() const;
    EditorWidget *editorAt(int index) const;

    // Motyw dla wszystkich kart (na czas sesji; trwały zapis w E3).
    void setTheme(ThemeId theme);
    ThemeId theme() const { return m_theme; }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createViewAndLanguageMenus();

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

    bool saveEditor(EditorWidget *editor, bool askForPath);
    bool resolveUnsavedChanges(EditorWidget *editor);
    bool closeTab(int index);

    EditorWidget *addEditorTab(DocumentId id, const QString &text, LineEnding lineEnding);
    EditorWidget *findEditor(DocumentId id) const;
    DocumentId documentIdOf(const EditorWidget *editor) const;
    bool isPristineUntitled(const EditorWidget *editor) const;
    QString displayName(DocumentId id) const;

    void updateTabTitle(EditorWidget *editor);
    void updateWindowTitle();
    void updateActions();
    void updateCursorStatus();
    void updateLanguageActions();

    void reportOpenFailure(const QString &path, const OpenOutcome &outcome);
    void reportSaveFailure(const QString &name, const SaveOutcome &outcome);
    static QString fileErrorText(FileError error, const QString &systemMessage);

    std::unique_ptr<UserPrompts> m_prompts;
    DocumentController m_documents;
    QHash<const EditorWidget *, DocumentId> m_documentIds;

    QTabWidget *m_tabs = nullptr;
    QLabel *m_cursorLabel = nullptr;

    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
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
    QAction *m_lightThemeAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QActionGroup *m_languageGroup = nullptr;
    QList<QAction *> m_languageActions; // QAction::data() = int(LanguageId)

    ThemeId m_theme = ThemeId::Light;
};

} // namespace cynamoncode
