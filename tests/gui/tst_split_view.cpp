// Podział widoku (D036; TESTY.md T22): przenoszenie kart między panelami,
// klon ze wspólnym buforem, zamykanie widoków bez utraty dokumentu.

#include "TestHelpers.h"

#include <QApplication>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>

using namespace cynamoncode;
using namespace cynamoncode::testing;

class TestSplitView : public QObject
{
    Q_OBJECT

private slots:
    void secondPaneHiddenUntilUsed();
    void moveTabToOtherView();
    void cloneSharesBufferAndHistory();
    void closingCloneKeepsDocument();
    void moveOntoExistingCloneMergesViews();
    void savingFromCloneUpdatesBothTabs();
    void languageChangeAppliesToAllViews();
    void externalChangeAsksOncePerDocument();
    void windowCloseAsksOncePerDocument();
    void cloneKeepsCrLfLineEndings();
    void activePaneFollowsKeyboardFocus();
};

void TestSplitView::cloneKeepsCrLfLineEndings()
{
    // Regresja przeglądu E4 (W1): setDocument() narzucał wspólnemu dokumentowi LF.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("windows.txt"));
    QVERIFY(writeBytes(path, "a\r\nb\r\n"));

    WindowFixture f;
    QVERIFY(f.window->openFile(path));
    EditorWidget *original = f.window->currentEditor();
    QVERIFY(f.window->cloneCurrentToOtherView());
    EditorWidget *clone = f.window->currentEditor();
    QVERIFY(original->eolMode() == QsciScintilla::EolWindows);
    QVERIFY(clone->eolMode() == QsciScintilla::EolWindows);

    for (EditorWidget *view : {original, clone}) {
        view->SendScintilla(QsciScintillaBase::SCI_DOCUMENTEND);
        view->SendScintilla(QsciScintillaBase::SCI_NEWLINE);
    }
    const QString text = clone->text();
    QCOMPARE(text, QStringLiteral("a\r\nb\r\n\r\n\r\n"));
    trigger(*f.window, "actionSave");
    QCOMPARE(readBytes(path), QByteArray("a\r\nb\r\n\r\n\r\n"));
}

void TestSplitView::activePaneFollowsKeyboardFocus()
{
    // Regresja przeglądu E4 (W2): zmiana karty w panelu bez fokusu nie może
    // przenieść skrótów (Ctrl+S, Zamień…) do innego dokumentu.
    WindowFixture f;
    f.window->show();
    f.window->activateWindow();
    if (!QTest::qWaitForWindowActive(f.window.get()))
        QSKIP("Platforma nie aktywuje okna — fokusu klawiatury nie da się sprawdzić.");

    edit(f.window->currentEditor(), QStringLiteral("lewy 1"));
    trigger(*f.window, "actionNew");
    edit(f.window->currentEditor(), QStringLiteral("lewy 2"));
    QVERIFY(f.window->moveCurrentToOtherView());
    trigger(*f.window, "actionNew"); // drugi dokument w panelu 2
    edit(f.window->currentEditor(), QStringLiteral("prawy 2"));

    // Fokus w panelu 1 (lewym).
    auto *left = qobject_cast<EditorWidget *>(f.window->pane(0)->widget(0));
    left->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget *>(left));
    QTRY_COMPARE(f.window->currentEditor(), left);

    // Programowa zmiana karty w prawym panelu — aktywny zostaje lewy.
    f.window->pane(1)->setCurrentIndex(0);
    QCOMPARE(f.window->activePane(), 0);
    QCOMPARE(f.window->currentEditor(), left);

    // Kliknięcie karty w prawym panelu: tam idą fokus i skróty.
    QTabBar *bar = f.window->pane(1)->tabBar();
    QTest::mouseClick(bar, Qt::LeftButton, {}, bar->tabRect(1).center());
    QTRY_COMPARE(f.window->activePane(), 1);
    QTRY_COMPARE(QApplication::focusWidget(),
                 static_cast<QWidget *>(f.window->pane(1)->widget(1)));
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("prawy 2"));
}

void TestSplitView::secondPaneHiddenUntilUsed()
{
    WindowFixture f;
    QVERIFY(f.window->pane(1)->isHidden());
    trigger(*f.window, "actionCloneToOtherView");
    QVERIFY(!f.window->pane(1)->isHidden());
    QCOMPARE(f.window->pane(1)->count(), 1);
    trigger(*f.window, "actionCloseTab"); // zamyka klon w panelu 2
    QVERIFY(f.window->pane(1)->isHidden());
    QCOMPARE(f.window->documentCount(), 1);
}

void TestSplitView::moveTabToOtherView()
{
    QTemporaryDir dir;
    const QString a = dir.filePath(QStringLiteral("a.txt"));
    const QString b = dir.filePath(QStringLiteral("b.txt"));
    QVERIFY(writeBytes(a, "a"));
    QVERIFY(writeBytes(b, "b"));

    WindowFixture f;
    QVERIFY(f.window->openFile(a));
    QVERIFY(f.window->openFile(b));
    QCOMPARE(f.window->pane(0)->count(), 2);

    QVERIFY(f.window->moveCurrentToOtherView());
    QCOMPARE(f.window->pane(0)->count(), 1);
    QCOMPARE(f.window->pane(1)->count(), 1);
    QCOMPARE(f.window->activePane(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("b"));
    QCOMPARE(f.window->documentCount(), 2);

    // Otwarcie pliku z drugiego panelu tylko go aktywuje.
    QVERIFY(f.window->openFile(a));
    QCOMPARE(f.window->documentCount(), 2);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("a"));

    // Powrót: drugi panel pustoszeje i chowa się.
    QVERIFY(f.window->openFile(b));
    QVERIFY(f.window->moveCurrentToOtherView());
    QCOMPARE(f.window->pane(0)->count(), 2);
    QVERIFY(f.window->pane(1)->isHidden());
}

void TestSplitView::cloneSharesBufferAndHistory()
{
    WindowFixture f;
    EditorWidget *original = f.window->currentEditor();
    edit(original, QStringLiteral("wspólny tekst"));
    QVERIFY(f.window->cloneCurrentToOtherView());
    EditorWidget *clone = f.window->currentEditor();
    QVERIFY(clone != original);
    QCOMPARE(f.window->activePane(), 1);
    QCOMPARE(clone->text(), QStringLiteral("wspólny tekst"));
    QVERIFY(clone->isModified());

    edit(clone, QStringLiteral("zmiana w klonie"));
    QCOMPARE(original->text(), QStringLiteral("zmiana w klonie"));
    original->undo();
    QCOMPARE(clone->text(), QStringLiteral("wspólny tekst"));
    QVERIFY(clone->autoCompletionSource() == QsciScintilla::AcsNone);
    QVERIFY(clone->callTipsStyle() == QsciScintilla::CallTipsNone);
}

void TestSplitView::closingCloneKeepsDocument()
{
    WindowFixture f;
    edit(f.window->currentEditor(), QStringLiteral("niezapisane"));
    QVERIFY(f.window->cloneCurrentToOtherView());
    QCOMPARE(f.window->documentCount(), 2);

    // Zamknięcie jednego widoku: bez pytania, dokument żyje w drugim.
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.prompts->unsavedQuestions, 0);
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("niezapisane"));

    // Ostatni widok: pytanie o zmiany; Anuluj zostawia kartę.
    f.prompts->unsavedAnswers = {UnsavedChoice::Cancel};
    trigger(*f.window, "actionCloseTab");
    QCOMPARE(f.prompts->unsavedQuestions, 1);
    QCOMPARE(f.window->documentCount(), 1);
}

void TestSplitView::moveOntoExistingCloneMergesViews()
{
    WindowFixture f;
    edit(f.window->currentEditor(), QStringLiteral("x"));
    QVERIFY(f.window->cloneCurrentToOtherView());
    QVERIFY(f.window->moveCurrentToOtherView()); // klon wraca do panelu z oryginałem
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("x"));
    QCOMPARE(f.prompts->unsavedQuestions, 0);
}

void TestSplitView::savingFromCloneUpdatesBothTabs()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "start"));

    WindowFixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(f.window->cloneCurrentToOtherView());
    edit(f.window->currentEditor(), QStringLiteral("zapisane z klonu"));
    QVERIFY(f.window->pane(0)->tabText(0).endsWith(QLatin1Char('*')));
    QVERIFY(f.window->pane(1)->tabText(0).endsWith(QLatin1Char('*')));

    trigger(*f.window, "actionSave");
    QCOMPARE(readBytes(path), QByteArray("zapisane z klonu"));
    QCOMPARE(f.window->pane(0)->tabText(0), QStringLiteral("a.txt"));
    QCOMPARE(f.window->pane(1)->tabText(0), QStringLiteral("a.txt"));
}

void TestSplitView::languageChangeAppliesToAllViews()
{
    WindowFixture f;
    QVERIFY(f.window->cloneCurrentToOtherView());
    trigger(*f.window, "actionLanguage_cpp");
    QCOMPARE(f.window->documentCount(), 2);
    for (int i = 0; i < f.window->documentCount(); ++i)
        QCOMPARE(int(f.window->editorAt(i)->language()), int(LanguageId::Cpp));
}

void TestSplitView::externalChangeAsksOncePerDocument()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "start"));

    WindowFixture f;
    QVERIFY(f.window->openFile(path));
    QVERIFY(f.window->cloneCurrentToOtherView());
    f.prompts->externalAnswers = {ExternalChangeChoice::Reload};
    QVERIFY(writeBytes(path, "zmiana z innego programu"));
    QTRY_COMPARE_WITH_TIMEOUT(f.prompts->externalQuestions, 1, 5000);
    QTest::qWait(FileWatchService::DebounceMs * 4);
    QCOMPARE(f.prompts->externalQuestions, 1);
    QCOMPARE(f.window->editorAt(0)->text(), QStringLiteral("zmiana z innego programu"));
    QCOMPARE(f.window->editorAt(1)->text(), QStringLiteral("zmiana z innego programu"));
}

void TestSplitView::windowCloseAsksOncePerDocument()
{
    WindowFixture f;
    f.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
    edit(f.window->currentEditor(), QStringLiteral("niezapisane"));
    QVERIFY(f.window->cloneCurrentToOtherView());
    f.prompts->unsavedAnswers = {UnsavedChoice::Cancel};
    QVERIFY(!f.window->close());
    QCOMPARE(f.prompts->unsavedQuestions, 1);
    QCOMPARE(f.window->documentCount(), 2);
}

QTEST_MAIN(TestSplitView)
#include "tst_split_view.moc"
