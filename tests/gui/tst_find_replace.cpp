// Znajdź i zamień w oknie głównym: zaznaczanie trafień (także z polskimi znakami
// i emoji), zamiana pojedyncza, „Zamień wszystkie” jako jedno cofnięcie (T17),
// okno dialogowe i skróty F3 (D028).

#include "editor/EditorWidget.h"
#include "ui/FindReplaceDialog.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>

using namespace cynamoncode;

namespace {

SearchOptions options(const QString &pattern, bool regex = false)
{
    SearchOptions result;
    result.pattern = pattern;
    result.regex = regex;
    return result;
}

void edit(EditorWidget *editor, const QString &text)
{
    editor->selectAll();
    editor->replaceSelectedText(text);
    editor->setCursorPosition(0, 0);
}

} // namespace

class TestFindReplace : public QObject
{
    Q_OBJECT

private slots:
    void findNextSelectsAndWraps();
    void findPreviousGoesBack();
    void findWorksWithPolishAndEmoji();
    void replaceSelectsThenReplaces();
    void replaceAllIsSingleUndoStep();
    void replaceAllWithRegexGroups();
    void bulkReplaceKeepsUnicodeAndUndo();
    void bulkReplaceKeepsCursorAndScroll();
    void invalidRegexShowsMessage();
    void dialogButtonsAndShortcuts();
    void selectionPrefillsDialog();
};

void TestFindReplace::findNextSelectsAndWraps()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("jeden dwa jeden"));

    QVERIFY(window.findInCurrent(options(QStringLiteral("jeden")), true));
    QCOMPARE(editor->selectedText(), QStringLiteral("jeden"));
    int lineFrom, indexFrom, lineTo, indexTo;
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(indexFrom, 0);

    QVERIFY(window.findInCurrent(options(QStringLiteral("jeden")), true));
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(indexFrom, 10);

    QVERIFY(window.findInCurrent(options(QStringLiteral("jeden")), true)); // zawinięcie
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(indexFrom, 0);

    QVERIFY(!window.findInCurrent(options(QStringLiteral("brak")), true));
    QCOMPARE(editor->text(), QStringLiteral("jeden dwa jeden"));
}

void TestFindReplace::findPreviousGoesBack()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("a x a x a"));
    editor->setCursorPosition(0, 9);
    QVERIFY(window.findInCurrent(options(QStringLiteral("a")), false));
    int lineFrom, indexFrom, lineTo, indexTo;
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(indexFrom, 8);
    QVERIFY(window.findInCurrent(options(QStringLiteral("a")), false));
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(indexFrom, 4);
}

void TestFindReplace::findWorksWithPolishAndEmoji()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("😀 żółw\nŻÓŁW 🐈 żółw"));

    QStringList found;
    for (int i = 0; i < 3; ++i) {
        QVERIFY(window.findInCurrent(options(QStringLiteral("żółw")), true));
        found << editor->selectedText();
    }
    QCOMPARE(found, (QStringList{QStringLiteral("żółw"), QStringLiteral("ŻÓŁW"),
                                 QStringLiteral("żółw")}));
}

void TestFindReplace::replaceSelectsThenReplaces()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("kot kot kot"));

    // Pierwsze „Zamień” tylko zaznacza (zaznaczenie nie było trafieniem).
    QVERIFY(window.replaceInCurrent(options(QStringLiteral("kot")), QStringLiteral("pies")));
    QCOMPARE(editor->text(), QStringLiteral("kot kot kot"));
    QVERIFY(window.replaceInCurrent(options(QStringLiteral("kot")), QStringLiteral("pies")));
    QCOMPARE(editor->text(), QStringLiteral("pies kot kot"));
    QCOMPARE(editor->selectedText(), QStringLiteral("kot"));
    QVERIFY(window.replaceInCurrent(options(QStringLiteral("kot")), QStringLiteral("pies")));
    QCOMPARE(editor->text(), QStringLiteral("pies pies kot"));
}

void TestFindReplace::replaceAllIsSingleUndoStep()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    const QString original = QStringLiteral("ala ma kota, ala ma psa, ALA ma rybki");
    edit(editor, original);

    QCOMPARE(window.replaceAllInCurrent(options(QStringLiteral("ala")), QStringLiteral("Ola")), 3);
    QCOMPARE(editor->text(), QStringLiteral("Ola ma kota, Ola ma psa, Ola ma rybki"));
    QVERIFY(editor->isModified());

    editor->undo();
    QCOMPARE(editor->text(), original);
    editor->redo();
    QCOMPARE(editor->text(), QStringLiteral("Ola ma kota, Ola ma psa, Ola ma rybki"));
}

void TestFindReplace::replaceAllWithRegexGroups()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("łódź=1\r\nżółw=2\r\n"));
    QCOMPARE(window.replaceAllInCurrent(options(QStringLiteral("^(\\w+)=(\\d)$"), true),
                                        QStringLiteral("\\2: \\1")),
             2);
    QCOMPARE(editor->text(), QStringLiteral("1: łódź\r\n2: żółw\r\n"));
    QVERIFY(editor->autoCompletionSource() == QsciScintilla::AcsNone);
}

void TestFindReplace::bulkReplaceKeepsUnicodeAndUndo()
{
    // ≥ 64 zamian idzie ścieżką jednej podmiany zakresu (EditorWidget::applyReplacements).
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    QString original = QStringLiteral("początek 🐈 ");
    QString expected = original;
    for (int i = 0; i < 300; ++i) {
        original += QStringLiteral("😀ż%1 łódź\n").arg(i);
        expected += QStringLiteral("[Ż]%1 łódź\n").arg(i);
    }
    original += QStringLiteral("koniec 🔥");
    expected += QStringLiteral("koniec 🔥");
    edit(editor, original);

    QCOMPARE(window.replaceAllInCurrent(options(QStringLiteral("😀ż")), QStringLiteral("[Ż]")), 300);
    QCOMPARE(editor->text(), expected);
    editor->undo();
    QCOMPARE(editor->text(), original);
    editor->redo();
    QCOMPARE(editor->text(), expected);
}

void TestFindReplace::bulkReplaceKeepsCursorAndScroll()
{
    // Regresja przeglądu E4 (D4): ścieżka zbiorcza nie przesuwa kursora ani widoku.
    MainWindow window;
    window.resize(800, 300);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    EditorWidget *editor = window.currentEditor();
    QString text;
    for (int i = 0; i < 200; ++i)
        text += QStringLiteral("kot %1\n").arg(i);
    edit(editor, text);
    editor->setCursorPosition(120, 2); // „ko|t 120” — wewnątrz trafienia
    editor->setFirstVisibleLine(100);

    QCOMPARE(window.replaceAllInCurrent(options(QStringLiteral("kot")), QStringLiteral("lis")), 200);
    int line = -1;
    int index = -1;
    editor->getCursorPosition(&line, &index);
    QCOMPARE(line, 120);
    QCOMPARE(index, 0); // wewnątrz trafienia → jego początek
    QCOMPARE(editor->firstVisibleLine(), 100);

    // Za trafieniem: „lis 1|50” — po zamianie na dłuższy tekst z polską literą
    // kursor stoi przed „50” (sprawdzane w indeksach QString, nie kolumnach Scintilli).
    editor->setCursorPosition(150, 5); // przewija widok do kursora
    const int scrolledTo = editor->firstVisibleLine();
    QCOMPARE(window.replaceAllInCurrent(options(QStringLiteral("lis")), QStringLiteral("rysiś")), 200);
    const QString after = editor->text();
    const auto [selectionStart, selectionEnd] = editor->selectionRange(after);
    QCOMPARE(selectionStart, selectionEnd);
    QCOMPARE(after.mid(selectionStart, 3), QStringLiteral("50\n"));
    QCOMPARE(after.mid(selectionStart - 7, 7), QStringLiteral("rysiś 1"));
    QCOMPARE(editor->firstVisibleLine(), scrolledTo);
}

void TestFindReplace::invalidRegexShowsMessage()
{
    MainWindow window;
    edit(window.currentEditor(), QStringLiteral("(abc"));
    FindReplaceDialog *dialog = window.findDialog();
    QVERIFY(!window.findInCurrent(options(QStringLiteral("(abc"), true), true));
    QVERIFY(!dialog->findChild<QLabel *>(QStringLiteral("statusLabel"))->text().isEmpty());
    QCOMPARE(window.currentEditor()->text(), QStringLiteral("(abc"));
}

void TestFindReplace::dialogButtonsAndShortcuts()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("raz dwa raz"));

    window.findChild<QAction *>(QStringLiteral("actionFind"))->trigger();
    FindReplaceDialog *dialog = window.findDialog();
    QVERIFY(dialog->isVisible());
    dialog->findChild<QLineEdit *>(QStringLiteral("findEdit"))->setText(QStringLiteral("raz"));
    dialog->findChild<QLineEdit *>(QStringLiteral("replaceEdit"))->setText(QStringLiteral("trzy"));

    QTest::mouseClick(dialog->findChild<QPushButton *>(QStringLiteral("findNextButton")),
                      Qt::LeftButton);
    QCOMPARE(editor->selectedText(), QStringLiteral("raz"));

    // F3 szuka dalej tym samym wzorcem.
    window.findChild<QAction *>(QStringLiteral("actionFindNext"))->trigger();
    int lineFrom, indexFrom, lineTo, indexTo;
    editor->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(indexFrom, 8);

    QTest::mouseClick(dialog->findChild<QPushButton *>(QStringLiteral("replaceAllButton")),
                      Qt::LeftButton);
    QCOMPARE(editor->text(), QStringLiteral("trzy dwa trzy"));
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("statusLabel"))->text().contains(
        QLatin1Char('2')));
}

void TestFindReplace::selectionPrefillsDialog()
{
    MainWindow window;
    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("zaznaczony tekst"));
    editor->setSelection(0, 0, 0, 10);
    window.findChild<QAction *>(QStringLiteral("actionReplace"))->trigger();
    QCOMPARE(window.findDialog()->options().pattern, QStringLiteral("zaznaczony"));
}

QTEST_MAIN(TestFindReplace)
#include "tst_find_replace.moc"
