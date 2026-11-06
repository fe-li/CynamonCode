// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// „Znajdź / Zamień we wszystkich otwartych” (D038): wyniki w panelu, przejście
// do trafienia, zamiana po potwierdzeniu, osobne cofnięcie w każdym dokumencie.

#include "TestHelpers.h"
#include "ui/FindReplaceDialog.h"
#include "ui/SearchResultsDock.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>

using namespace cynamoncode;
using namespace cynamoncode::testing;

namespace {

SearchOptions options(const QString &pattern)
{
    SearchOptions result;
    result.pattern = pattern;
    return result;
}

// Trzy dokumenty: dwa z trafieniami, jeden bez.
void prepare(WindowFixture &f)
{
    edit(f.window->editorAt(0), QStringLiteral("kot\npies\nkot i kot\n"));
    trigger(*f.window, "actionNew");
    edit(f.window->editorAt(1), QStringLiteral("brak trafień\n"));
    trigger(*f.window, "actionNew");
    edit(f.window->editorAt(2), QStringLiteral("łódź\nKOT na końcu"));
}

} // namespace

class TestFindInAll : public QObject
{
    Q_OBJECT

private slots:
    void resultsListDocumentsAndLines();
    void activatingHitSelectsIt();
    void replaceInAllAfterConfirmation();
    void declinedReplaceChangesNothing();
    void cloneCountsOnce();
    void dialogButtonsWork();
    void lineNumbersMatchEditorForAllLineEndings();
};

void TestFindInAll::lineNumbersMatchEditorForAllLineEndings()
{
    // Regresja przeglądu E4 (D9): samotne CR i CRLF liczone jak w Scintilli.
    WindowFixture f;
    edit(f.window->currentEditor(), QStringLiteral("a\rkot\r\nb\nkot\rc kot"));
    QCOMPARE(f.window->findInAllDocuments(options(QStringLiteral("kot"))), 3);
    QTreeWidgetItem *document = f.window->searchResults()->tree()->topLevelItem(0);
    QCOMPARE(document->child(0)->text(0), QStringLiteral("2"));
    QCOMPARE(document->child(1)->text(0), QStringLiteral("4"));
    QCOMPARE(document->child(2)->text(0), QStringLiteral("5"));
    QCOMPARE(document->child(2)->text(1), QStringLiteral("c kot"));
    // Zgodność z numeracją edytora.
    QCOMPARE(f.window->currentEditor()->lines(), 5);
}

void TestFindInAll::resultsListDocumentsAndLines()
{
    WindowFixture f;
    prepare(f);
    QCOMPARE(f.window->findInAllDocuments(options(QStringLiteral("kot"))), 4);

    QTreeWidget *tree = f.window->searchResults()->tree();
    QVERIFY(!f.window->searchResults()->isHidden());
    QCOMPARE(tree->topLevelItemCount(), 2);
    QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("Nowy 1 (3)"));
    QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QStringLiteral("1"));
    QCOMPARE(tree->topLevelItem(0)->child(1)->text(0), QStringLiteral("3"));
    QCOMPARE(tree->topLevelItem(0)->child(1)->text(1), QStringLiteral("kot i kot"));
    QCOMPARE(tree->topLevelItem(1)->text(0), QStringLiteral("Nowy 3 (1)"));
    QCOMPARE(tree->topLevelItem(1)->child(0)->text(0), QStringLiteral("2"));

    auto *summary = f.window->searchResults()->findChild<QLabel *>(QStringLiteral("searchSummaryLabel"));
    QVERIFY(summary->text().contains(QStringLiteral("4")));

    QCOMPARE(f.window->findInAllDocuments(options(QStringLiteral("żyrafa"))), 0);
    QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestFindInAll::activatingHitSelectsIt()
{
    WindowFixture f;
    prepare(f);
    f.window->findInAllDocuments(options(QStringLiteral("kot")));
    QTreeWidget *tree = f.window->searchResults()->tree();

    emit tree->itemActivated(tree->topLevelItem(1)->child(0), 0);
    QCOMPARE(f.window->currentEditor(), f.window->editorAt(2));
    QCOMPARE(f.window->currentEditor()->selectedText(), QStringLiteral("KOT"));

    emit tree->itemActivated(tree->topLevelItem(0)->child(2), 0);
    QCOMPARE(f.window->currentEditor(), f.window->editorAt(0));
    int lineFrom, indexFrom, lineTo, indexTo;
    f.window->currentEditor()->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
    QCOMPARE(lineFrom, 2);
    QCOMPARE(indexFrom, 6);
}

void TestFindInAll::replaceInAllAfterConfirmation()
{
    WindowFixture f;
    prepare(f);
    QCOMPARE(f.window->replaceInAllDocuments(options(QStringLiteral("kot")), QStringLiteral("żbik")), 4);
    QCOMPARE(f.prompts->replaceInAllQuestions, 1);
    QCOMPARE(f.prompts->lastReplaceMatches, 4);
    QCOMPARE(f.prompts->lastReplaceDocuments, 2);
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("żbik\npies\nżbik i żbik\n"));
    QCOMPARE(f.window->editorAt(2)->text(), QStringLiteral("łódź\nżbik na końcu"));
    QCOMPARE(f.window->editorAt(1)->text(), QStringLiteral("brak trafień\n"));

    // Każdy dokument cofa się osobno, jednym krokiem.
    f.window->editorAt(2)->undo();
    QCOMPARE(f.window->editorAt(2)->text(), QStringLiteral("łódź\nKOT na końcu"));
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("żbik\npies\nżbik i żbik\n"));

    // Brak trafień — bez pytania.
    QCOMPARE(f.window->replaceInAllDocuments(options(QStringLiteral("żyrafa")), QStringLiteral("x")), 0);
    QCOMPARE(f.prompts->replaceInAllQuestions, 1);
}

void TestFindInAll::declinedReplaceChangesNothing()
{
    WindowFixture f;
    prepare(f);
    f.prompts->replaceInAllAnswer = false;
    QCOMPARE(f.window->replaceInAllDocuments(options(QStringLiteral("kot")), QStringLiteral("x")), 0);
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("kot\npies\nkot i kot\n"));
}

void TestFindInAll::cloneCountsOnce()
{
    WindowFixture f;
    edit(f.window->currentEditor(), QStringLiteral("kot kot"));
    QVERIFY(f.window->cloneCurrentToOtherView());
    QCOMPARE(f.window->findInAllDocuments(options(QStringLiteral("kot"))), 2);
    QCOMPARE(f.window->searchResults()->tree()->topLevelItemCount(), 1);
    QCOMPARE(f.window->replaceInAllDocuments(options(QStringLiteral("kot")), QStringLiteral("pies")), 2);
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("pies pies"));
    QCOMPARE(f.window->editorAt(1)->text(), QStringLiteral("pies pies"));
}

void TestFindInAll::dialogButtonsWork()
{
    WindowFixture f;
    f.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
    prepare(f);
    FindReplaceDialog *dialog = f.window->findDialog();
    dialog->setPattern(QStringLiteral("kot"));
    dialog->findChild<QLineEdit *>(QStringLiteral("replaceEdit"))->setText(QStringLiteral("lis"));

    QTest::mouseClick(dialog->findChild<QPushButton *>(QStringLiteral("findInAllButton")),
                      Qt::LeftButton);
    QCOMPARE(f.window->searchResults()->tree()->topLevelItemCount(), 2);

    QTest::mouseClick(dialog->findChild<QPushButton *>(QStringLiteral("replaceInAllButton")),
                      Qt::LeftButton);
    QCOMPARE(f.window->editorAt(2)->text(), QStringLiteral("łódź\nlis na końcu"));

    // Menu Szukaj → Znajdź we wszystkich (Ctrl+Shift+F) z wzorcem z okna.
    dialog->setPattern(QStringLiteral("lis"));
    trigger(*f.window, "actionFindInAll");
    QCOMPARE(f.window->searchResults()->tree()->topLevelItemCount(), 2);
}

QTEST_MAIN(TestFindInAll)
#include "tst_find_in_all.moc"
