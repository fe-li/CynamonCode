// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Integracja okna głównego: karty, otwieranie i zapis, ochrona niezapisanych
// zmian (TESTY.md T03–T08, T19; bramka E1). Dialogi zastępuje ScriptedPrompts.

#include "ScriptedPrompts.h"
#include "editor/EditorWidget.h"
#include "ui/MainWindow.h"
#include "ui/UserPrompts.h"

#include <QAction>
#include <QLabel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

#include <memory>
#include <optional>
#include <utility>

using namespace cynamoncode;

namespace {

using testing::ScriptedPrompts;

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

bool writeBytes(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

// Edycja jak przez użytkownika (wchodzi do historii cofania i ustawia isModified).
// setText()/append() QScintilli czyszczą historię i NIE oznaczają dokumentu jako zmienionego.
void edit(EditorWidget *editor, const QString &text)
{
    editor->selectAll();
    editor->replaceSelectedText(text);
}

void typeAndExpectNoSuggestions(EditorWidget *editor)
{
    editor->setFocus();
    QTest::keyClicks(editor, QStringLiteral("abc abc ab v."));
    QTest::keyClicks(editor, QStringLiteral("f("));
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    QVERIFY(!editor->isListActive());
    QVERIFY(!editor->isCallTipActive());
}

void trigger(MainWindow &window, const char *actionName)
{
    auto *action = window.findChild<QAction *>(QLatin1String(actionName));
    QVERIFY2(action, actionName);
    QVERIFY2(action->isEnabled(), actionName);
    action->trigger();
}

struct Fixture
{
    ScriptedPrompts *prompts = nullptr;
    std::unique_ptr<MainWindow> window;

    Fixture()
    {
        auto scripted = std::make_unique<ScriptedPrompts>();
        prompts = scripted.get();
        window = std::make_unique<MainWindow>(std::move(scripted));
        window->show();
    }
};

} // namespace

#define VERIFY_NO_SUGGESTIONS(editor)                                                    \
    do {                                                                                 \
        QVERIFY((editor)->autoCompletionSource() == QsciScintilla::AcsNone);             \
        QVERIFY((editor)->autoCompletionThreshold() < 0);                                \
        QVERIFY((editor)->callTipsStyle() == QsciScintilla::CallTipsNone);               \
    } while (false)

class TestTabs : public QObject
{
    Q_OBJECT

private slots:
    void startsWithOneUntitledTab();
    void everyTabHasSuggestionsDisabled();
    void undoRedoAndClipboardActions();
    void cancelKeepsModifiedTab();
    void discardClosesModifiedTab();
    void saveCloseReopenKeepsPolishText();
    void openingSameFileActivatesExistingTab();
    void sameNameInDifferentDirsGivesTwoTabs();
    void openingReplacesPristineUntitledTab();
    void invalidFileIsRefusedWithMessage();
    void failedSaveKeepsModifiedAndBlocksClose();
    void windowCloseCancelProtectsAllDocuments();
    void cancelledSaveAsKeepsDocumentModified();
    void goToLineMovesCursor();
    void openedFileGetsDetectedLanguage();
    void languageMenuChangesOnlyCurrentTab();
    void saveAsRedetectsLanguage();
    void manualLanguageSurvivesSaveAs();
    void mixedLineEndingsKeepByDefault();
    void mixedLineEndingsCanBeUnified();
    void themeAppliesToAllTabs();
    void indentationStyleFollowsFile();
    void saveAsSuggestsExtensionForLanguage();
    void savedHtmlWithoutDotIsColoredAfterReopen();
    void statusBarShowsDocumentInfo();
};

void TestTabs::startsWithOneUntitledTab()
{
    Fixture f;
    QCOMPARE(f.window->documentCount(), 1);
    QVERIFY(f.window->currentEditor() != nullptr);
    QVERIFY(!f.window->currentEditor()->isModified());
}

void TestTabs::everyTabHasSuggestionsDisabled()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("kod.cpp"));
    QVERIFY(writeBytes(path, "int main() { return 0; }\n"));

    Fixture f;
    QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
    trigger(*f.window, "actionNew");
    trigger(*f.window, "actionNew");
    QVERIFY(f.window->openFile(path));
    QCOMPARE(f.window->documentCount(), 4);

    for (int i = 0; i < f.window->documentCount(); ++i) {
        EditorWidget *editor = f.window->editorAt(i);
        QVERIFY(editor);
        VERIFY_NO_SUGGESTIONS(editor);
        typeAndExpectNoSuggestions(editor);
        VERIFY_NO_SUGGESTIONS(editor);
    }

    // Ponowne otwarcie zamkniętego pliku — nowa instancja edytora też bez podpowiedzi.
    f.prompts->unsavedAnswers = {UnsavedChoice::Discard};
    trigger(*f.window, "actionCloseTab");
    QVERIFY(f.window->openFile(path));
    VERIFY_NO_SUGGESTIONS(f.window->currentEditor());
    typeAndExpectNoSuggestions(f.window->currentEditor());
}

void TestTabs::undoRedoAndClipboardActions()
{
    Fixture f;
    EditorWidget *editor = f.window->currentEditor();
    edit(editor, QStringLiteral("pierwsza"));
    edit(editor, QStringLiteral("druga"));

    trigger(*f.window, "actionUndo");
    QCOMPARE(editor->text(), QStringLiteral("pierwsza"));
    trigger(*f.window, "actionRedo");
    QCOMPARE(editor->text(), QStringLiteral("druga"));

    trigger(*f.window, "actionSelectAll");
    QCOMPARE(editor->selectedText(), QStringLiteral("druga"));
    trigger(*f.window, "actionCopy");
    trigger(*f.window, "actionCut");
    QCOMPARE(editor->text(), QString());
    trigger(*f.window, "actionPaste");
    trigger(*f.window, "actionPaste");
    QCOMPARE(editor->text(), QStringLiteral("drugadruga"));
}

void TestTabs::cancelKeepsModifiedTab()
{
    Fixture f;
    EditorWidget *editor = f.window->currentEditor();
    edit(editor, QStringLiteral("ważny tekst"));
    QVERIFY(editor->isModified());

    f.prompts->unsavedAnswers = {UnsavedChoice::Cancel};
    trigger(*f.window, "actionCloseTab");

    QCOMPARE(f.prompts->unsavedQuestions, 1);
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("ważny tekst"));
    QVERIFY(f.window->currentEditor()->isModified());
}

void TestTabs::discardClosesModifiedTab()
{
    Fixture f;
    edit(f.window->currentEditor(), QStringLiteral("do wyrzucenia"));
    QVERIFY(f.window->currentEditor()->isModified());
    f.prompts->unsavedAnswers = {UnsavedChoice::Discard};
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.prompts->unsavedQuestions, 1);
    QCOMPARE(f.window->documentCount(), 0);
}

void TestTabs::saveCloseReopenKeepsPolishText()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("polski.txt"));
    const QString text = QStringLiteral("Zażółć gęślą jaźń 😀\nŻÓŁW ąęćłńóśźż\n");

    Fixture f;
    edit(f.window->currentEditor(), text);
    f.prompts->savePaths = {path};
    trigger(*f.window, "actionSave");

    QVERIFY(f.prompts->errors.isEmpty());
    QVERIFY(!f.window->currentEditor()->isModified());
    QCOMPARE(readBytes(path), text.toUtf8());

    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.prompts->unsavedQuestions, 0);
    QCOMPARE(f.window->documentCount(), 0);

    QVERIFY(f.window->openFile(path));
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), text);
    QVERIFY(!f.window->currentEditor()->isModified());
}

void TestTabs::openingSameFileActivatesExistingTab()
{
    QTemporaryDir dir;
    const QString first = dir.filePath(QStringLiteral("a.txt"));
    const QString second = dir.filePath(QStringLiteral("b.txt"));
    QVERIFY(writeBytes(first, "a"));
    QVERIFY(writeBytes(second, "b"));

    Fixture f;
    QVERIFY(f.window->openFile(first));
    EditorWidget *firstEditor = f.window->currentEditor();
    QVERIFY(f.window->openFile(second));
    QCOMPARE(f.window->documentCount(), 2);

    QVERIFY(f.window->openFile(first));
    QCOMPARE(f.window->documentCount(), 2);
    QCOMPARE(f.window->currentEditor(), firstEditor);
}

void TestTabs::sameNameInDifferentDirsGivesTwoTabs()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("x")));
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("y")));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("x/plik.txt")), "x"));
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("y/plik.txt")), "y"));

    Fixture f;
    QVERIFY(f.window->openFile(dir.filePath(QStringLiteral("x/plik.txt"))));
    QVERIFY(f.window->openFile(dir.filePath(QStringLiteral("y/plik.txt"))));
    QCOMPARE(f.window->documentCount(), 2);
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("x"));
    QCOMPARE(f.window->editorAt(1)->text(), QStringLiteral("y"));
}

void TestTabs::openingReplacesPristineUntitledTab()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "treść"));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("treść"));
}

void TestTabs::invalidFileIsRefusedWithMessage()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("latin1.txt"));
    const QByteArray bytes("za\xBF\xF3\xB3" "\xE6");
    QVERIFY(writeBytes(path, bytes));

    Fixture f;
    QVERIFY(!f.window->openFile(path));
    QCOMPARE(f.prompts->errors.size(), 1);
    QCOMPARE(f.window->documentCount(), 1); // tylko startowa karta
    QCOMPARE(readBytes(path), bytes);
}

void TestTabs::failedSaveKeepsModifiedAndBlocksClose()
{
    if (::geteuid() == 0)
        QSKIP("Użytkownik root omija prawa dostępu.");

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("tylko-odczyt.txt"));
    QVERIFY(writeBytes(path, "oryginał"));
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser));

    Fixture f;
    QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
    QVERIFY(f.window->openFile(path));
    EditorWidget *editor = f.window->currentEditor();
    editor->insert(QStringLiteral(" zmiana"));
    QVERIFY(editor->isModified());

    trigger(*f.window, "actionSave");
    QCOMPARE(f.prompts->errors.size(), 1);
    QVERIFY(editor->isModified());
    QCOMPARE(readBytes(path), QByteArray("oryginał"));

    // Zamknięcie okna z odpowiedzią „Zapisz” przy nieudanym zapisie nie może zgubić zmian.
    f.prompts->unsavedAnswers = {UnsavedChoice::Save};
    QVERIFY(!f.window->close());
    QVERIFY(f.window->isVisible());
    QVERIFY(editor->isModified());
    QCOMPARE(f.window->documentCount(), 1);
}

void TestTabs::windowCloseCancelProtectsAllDocuments()
{
    Fixture f;
    QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
    edit(f.window->currentEditor(), QStringLiteral("pierwszy"));
    trigger(*f.window, "actionNew");
    edit(f.window->currentEditor(), QStringLiteral("drugi"));
    QVERIFY(f.window->editorAt(0)->isModified());
    QVERIFY(f.window->editorAt(1)->isModified());

    f.prompts->unsavedAnswers = {UnsavedChoice::Discard, UnsavedChoice::Cancel};
    QVERIFY(!f.window->close());

    QCOMPARE(f.prompts->unsavedQuestions, 2);
    QVERIFY(f.window->isVisible());
    QCOMPARE(f.window->documentCount(), 2);
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("pierwszy"));
    QCOMPARE(f.window->editorAt(1)->text(), QStringLiteral("drugi"));
    QVERIFY(f.window->editorAt(0)->isModified());
    QVERIFY(f.window->editorAt(1)->isModified());
}

void TestTabs::cancelledSaveAsKeepsDocumentModified()
{
    Fixture f;
    edit(f.window->currentEditor(), QStringLiteral("tekst"));
    QVERIFY(f.window->currentEditor()->isModified());
    // Brak ścieżki w skrypcie = użytkownik anulował dialog zapisu.
    f.prompts->unsavedAnswers = {UnsavedChoice::Save};
    trigger(*f.window, "actionCloseTab");

    QCOMPARE(f.window->documentCount(), 1);
    QVERIFY(f.window->currentEditor()->isModified());
    QVERIFY(f.prompts->errors.isEmpty());
}

void TestTabs::goToLineMovesCursor()
{
    Fixture f;
    f.window->currentEditor()->setText(QStringLiteral("1\n2\n3\n4\n5\n"));
    f.prompts->lineNumber = 3;
    trigger(*f.window, "actionGoToLine");

    int line = -1;
    int index = -1;
    f.window->currentEditor()->getCursorPosition(&line, &index);
    QCOMPARE(line, 2);
    QCOMPARE(index, 0);
}

void TestTabs::openedFileGetsDetectedLanguage()
{
    QTemporaryDir dir;
    const QString python = dir.filePath(QStringLiteral("skrypt.py"));
    const QString shebang = dir.filePath(QStringLiteral("uruchom"));
    QVERIFY(writeBytes(python, "def f():\n    pass\n"));
    QVERIFY(writeBytes(shebang, "#!/bin/bash\necho ok\n"));

    Fixture f;
    QVERIFY(f.window->openFile(python));
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Python));
    QVERIFY(f.window->findChild<QAction *>(QStringLiteral("actionLanguage_python"))->isChecked());

    QVERIFY(f.window->openFile(shebang));
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Bash));
    VERIFY_NO_SUGGESTIONS(f.window->currentEditor());
}

void TestTabs::languageMenuChangesOnlyCurrentTab()
{
    Fixture f;
    trigger(*f.window, "actionNew");
    EditorWidget *second = f.window->currentEditor();
    edit(second, QStringLiteral("SELECT 1;"));

    trigger(*f.window, "actionLanguage_sql");
    QCOMPARE(int(second->language()), int(LanguageId::Sql));
    QCOMPARE(int(f.window->editorAt(0)->language()), int(LanguageId::Plain));
    QCOMPARE(second->text(), QStringLiteral("SELECT 1;"));
    QVERIFY(second->isModified());
    VERIFY_NO_SUGGESTIONS(second);

    // Menu pokazuje język aktywnej karty.
    trigger(*f.window, "actionCloseTab"); // pyta o zmiany → brak odpowiedzi = Anuluj
    QCOMPARE(f.window->documentCount(), 2);
    f.prompts->unsavedAnswers = {UnsavedChoice::Discard};
    trigger(*f.window, "actionCloseTab");
    QVERIFY(f.window->findChild<QAction *>(QStringLiteral("actionLanguage_plain"))->isChecked());
}

void TestTabs::saveAsRedetectsLanguage()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("program.cpp"));

    Fixture f;
    edit(f.window->currentEditor(), QStringLiteral("int main() {}\n"));
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Plain));
    f.prompts->savePaths = {path};
    trigger(*f.window, "actionSave");
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Cpp));
    QVERIFY(f.window->findChild<QAction *>(QStringLiteral("actionLanguage_cpp"))->isChecked());
}

void TestTabs::manualLanguageSurvivesSaveAs()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("dane.txt"));

    Fixture f;
    edit(f.window->currentEditor(), QStringLiteral("{\"a\": 1}"));
    trigger(*f.window, "actionLanguage_json");
    f.prompts->savePaths = {path};
    trigger(*f.window, "actionSave");
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Json));
}

void TestTabs::mixedLineEndingsKeepByDefault()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("mieszane.txt"));
    const QByteArray bytes("a\nb\r\nc\n");
    QVERIFY(writeBytes(path, bytes));

    Fixture f;
    QVERIFY(f.window->openFile(path));
    QCOMPARE(f.prompts->mixedQuestions, 1);
    EditorWidget *editor = f.window->currentEditor();
    QVERIFY(!editor->isModified());
    QCOMPARE(editor->text(), QString::fromUtf8(bytes));

    // Ponowna aktywacja otwartego pliku nie pyta drugi raz.
    QVERIFY(f.window->openFile(path));
    QCOMPARE(f.prompts->mixedQuestions, 1);

    edit(editor, editor->text()); // zmiana bez zmiany treści oznacza dokument
    trigger(*f.window, "actionSave");
    QCOMPARE(readBytes(path), bytes);
}

void TestTabs::mixedLineEndingsCanBeUnified()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("mieszane.txt"));
    QVERIFY(writeBytes(path, "a\nb\r\nc\rd"));

    Fixture f;
    f.prompts->mixedAnswer = MixedLineEndingChoice::ConvertToCrLf;
    QVERIFY(f.window->openFile(path));
    EditorWidget *editor = f.window->currentEditor();
    QVERIFY(editor->isModified());
    QCOMPARE(editor->text(), QStringLiteral("a\r\nb\r\nc\r\nd"));
    // Plik na dysku zmienia się dopiero po zapisie (D020).
    QCOMPARE(readBytes(path), QByteArray("a\nb\r\nc\rd"));

    trigger(*f.window, "actionSave");
    QCOMPARE(readBytes(path), QByteArray("a\r\nb\r\nc\r\nd"));
    QVERIFY(!editor->isModified());
}

void TestTabs::themeAppliesToAllTabs()
{
    Fixture f;
    trigger(*f.window, "actionNew");
    trigger(*f.window, "actionLanguage_python");

    trigger(*f.window, "actionThemeDark");
    QVERIFY(f.window->theme() == ThemeId::Dark);
    for (int i = 0; i < f.window->documentCount(); ++i) {
        EditorWidget *editor = f.window->editorAt(i);
        QVERIFY(editor->theme().id == ThemeId::Dark);
        VERIFY_NO_SUGGESTIONS(editor);
    }
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Python));

    // Nowa karta dziedziczy bieżący motyw.
    trigger(*f.window, "actionNew");
    QVERIFY(f.window->currentEditor()->theme().id == ThemeId::Dark);

    trigger(*f.window, "actionThemeLight");
    QVERIFY(f.window->editorAt(0)->theme().id == ThemeId::Light);
}

void TestTabs::indentationStyleFollowsFile()
{
    QTemporaryDir dir;
    const QString tabs = dir.filePath(QStringLiteral("tabulatory.c"));
    const QString spaces = dir.filePath(QStringLiteral("spacje.c"));
    QVERIFY(writeBytes(tabs, "int f() {\n\treturn 0;\n}\n"));
    QVERIFY(writeBytes(spaces, "int f() {\n    return 0;\n}\n"));

    Fixture f;
    QVERIFY(!f.window->currentEditor()->indentationsUseTabs()); // nowy dokument: spacje
    QVERIFY(f.window->openFile(tabs));
    QVERIFY(f.window->currentEditor()->indentationsUseTabs());
    QVERIFY(f.window->currentEditor()->autoIndent());
    QVERIFY(f.window->openFile(spaces));
    QVERIFY(!f.window->currentEditor()->indentationsUseTabs());
}

void TestTabs::saveAsSuggestsExtensionForLanguage()
{
    Fixture f;
    edit(f.window->currentEditor(), QStringLiteral("<html></html>"));
    trigger(*f.window, "actionLanguage_html");
    trigger(*f.window, "actionSave"); // brak ścieżki w skrypcie = anulowanie dialogu
    QVERIFY2(f.prompts->lastSuggestedPath.endsWith(QStringLiteral("/Nowy 1.html")),
             qPrintable(f.prompts->lastSuggestedPath));

    trigger(*f.window, "actionNew"); // zwykły tekst — bez rozszerzenia
    edit(f.window->currentEditor(), QStringLiteral("tekst"));
    trigger(*f.window, "actionSave");
    QVERIFY2(f.prompts->lastSuggestedPath.endsWith(QStringLiteral("/Nowy 2")),
             qPrintable(f.prompts->lastSuggestedPath));
}

void TestTabs::savedHtmlWithoutDotIsColoredAfterReopen()
{
    // Odtworzenie zgłoszenia: zapis jako „Nowy 1html” (bez kropki) i ponowne otwarcie.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("Nowy 1html"));

    Fixture f;
    edit(f.window->currentEditor(), QStringLiteral("<html>\n    <body></body>\n</html>\n"));
    trigger(*f.window, "actionLanguage_html");
    f.prompts->savePaths = {path};
    trigger(*f.window, "actionSave");
    QVERIFY(f.prompts->errors.isEmpty());
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.window->documentCount(), 0);

    QVERIFY(f.window->openFile(path));
    QCOMPARE(int(f.window->currentEditor()->language()), int(LanguageId::Html));
    QVERIFY(f.window->currentEditor()->lexer() != nullptr);
}

void TestTabs::statusBarShowsDocumentInfo()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("skrypt.py"));
    QVERIFY(writeBytes(path, QByteArray("\xEF\xBB\xBF", 3) + "\tżółw = 1\r\nx = 2\r\n"));

    Fixture f;
    const auto label = [&f](const char *name) {
        return f.window->findChild<QLabel *>(QLatin1String(name))->text();
    };
    // Nowy dokument.
    QCOMPARE(label("languageLabel"), QStringLiteral("Zwykły tekst"));
    QCOMPARE(label("encodingLabel"), QStringLiteral("UTF-8"));
    QCOMPARE(label("lineEndingLabel"), QStringLiteral("LF"));
    QCOMPARE(label("cursorLabel"), QStringLiteral("Wiersz 1, kolumna 1"));

    QVERIFY(f.window->openFile(path));
    QCOMPARE(label("languageLabel"), QStringLiteral("Python"));
    QCOMPARE(label("encodingLabel"), QStringLiteral("UTF-8 z BOM"));
    QCOMPARE(label("lineEndingLabel"), QStringLiteral("CRLF"));

    // Kolumna w znakach, tabulator = 4 kolumny: "\tżółw = 1" (9 znaków) → koniec w kolumnie 13.
    EditorWidget *editor = f.window->currentEditor();
    editor->setCursorPosition(0, 0);
    editor->SendScintilla(QsciScintillaBase::SCI_LINEEND);
    QCOMPARE(label("cursorLabel"), QStringLiteral("Wiersz 1, kolumna 13"));
    editor->SendScintilla(QsciScintillaBase::SCI_HOME);
    editor->SendScintilla(QsciScintillaBase::SCI_LINEENDEXTEND);
    QCOMPARE(label("cursorLabel"), QStringLiteral("Wiersz 1, kolumna 13, zaznaczono 9"));

    trigger(*f.window, "actionLanguage_plain");
    QCOMPARE(label("languageLabel"), QStringLiteral("Zwykły tekst"));
}

QTEST_MAIN(TestTabs)
#include "tst_tabs.moc"
