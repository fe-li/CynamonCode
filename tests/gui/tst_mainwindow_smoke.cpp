// Smoke test GUI: główne okno powstaje, ma tytuł produktu, pokazuje się
// i zamyka akcją „Zakończ”. Bez ekranu: QT_QPA_PLATFORM=offscreen.

#include "ui/MainWindow.h"

#include <QAction>
#include <QKeySequence>
#include <QtTest>

class TestMainWindowSmoke : public QObject
{
    Q_OBJECT

private slots:
    void hasProductTitle();
    void showsAndCloses();
    void quitActionClosesWindow();
};

void TestMainWindowSmoke::hasProductTitle()
{
    cynamoncode::MainWindow window;
    QCOMPARE(window.windowTitle(), QStringLiteral("CynamonCode"));
}

void TestMainWindowSmoke::showsAndCloses()
{
    cynamoncode::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.close());
    QVERIFY(!window.isVisible());
}

void TestMainWindowSmoke::quitActionClosesWindow()
{
    cynamoncode::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *quitAction = window.findChild<QAction *>(QStringLiteral("actionQuit"));
    QVERIFY(quitAction != nullptr);
    QCOMPARE(quitAction->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Q));

    quitAction->trigger();
    QVERIFY(!window.isVisible());
}

QTEST_MAIN(TestMainWindowSmoke)
#include "tst_mainwindow_smoke.moc"
