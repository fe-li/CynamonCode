// Menu Pomoc → „O programie”: logo, nazwa, wersja, autor, strona www;
// ikona aplikacji we wszystkich rozmiarach z zasobów Qt.

#include "app/AppInfo.h"
#include "ui/AboutDialog.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QtTest>

using namespace cynamoncode;

class TestAbout : public QObject
{
    Q_OBJECT

private slots:
    void helpMenuHasAboutAction();
    void aboutDialogShowsProgramInfo();
    void applicationIconHasAllSizes();
};

void TestAbout::helpMenuHasAboutAction()
{
    MainWindow window;
    auto *help = window.findChild<QMenu *>(QStringLiteral("menuHelp"));
    QVERIFY(help);
    QCOMPARE(help->title(), QStringLiteral("Pomo&c"));
    // Pomoc jest ostatnim menu.
    QCOMPARE(window.menuBar()->actions().last(), help->menuAction());
    auto *about = window.findChild<QAction *>(QStringLiteral("actionAbout"));
    QVERIFY(about);
    QVERIFY(about->text().contains(QStringLiteral("CynamonCode")));
}

void TestAbout::aboutDialogShowsProgramInfo()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.findChild<QAction *>(QStringLiteral("actionAbout"))->trigger();

    auto *dialog = window.findChild<AboutDialog *>(QStringLiteral("aboutDialog"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());

    const auto text = [dialog](const char *name) {
        auto *label = dialog->findChild<QLabel *>(QLatin1String(name));
        return label ? label->text() : QString();
    };
    QCOMPARE(text("nameLabel"), QStringLiteral("CynamonCode"));
    QCOMPARE(text("versionLabel"), QStringLiteral("Wersja %1").arg(AppInfo::version()));
    QCOMPARE(AppInfo::version(), QStringLiteral("0.1.0"));
    QCOMPARE(text("authorLabel"), QStringLiteral("Autor: Kamil Feliszewski"));
    QVERIFY(text("websiteLabel").contains(QStringLiteral("href=\"https://www.feliszewski.pl\"")));
    QVERIFY(text("websiteLabel").contains(QStringLiteral("www.feliszewski.pl</a>")));
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("websiteLabel"))->openExternalLinks());
    QVERIFY(text("librariesLabel").contains(QStringLiteral("QScintilla")));
    QVERIFY(text("licenseLabel").contains(QStringLiteral("GPL-3.0-or-later")));

    auto *logo = dialog->findChild<QLabel *>(QStringLiteral("logoLabel"));
    QVERIFY(logo);
    QVERIFY(!logo->pixmap().isNull());
    QCOMPARE(logo->pixmap().width() / logo->pixmap().devicePixelRatio(), 128.0);

    // Drugie otwarcie używa tego samego okna.
    dialog->close();
    window.findChild<QAction *>(QStringLiteral("actionAbout"))->trigger();
    QCOMPARE(window.findChildren<AboutDialog *>().size(), qsizetype(1));
}

void TestAbout::applicationIconHasAllSizes()
{
    const QIcon icon = applicationIcon();
    QVERIFY(!icon.isNull());
    const QList<QSize> sizes = icon.availableSizes();
    for (const int size : {16, 24, 32, 48, 64, 128, 256, 512})
        QVERIFY2(sizes.contains(QSize(size, size)), qPrintable(QString::number(size)));
    QVERIFY(!icon.pixmap(QSize(48, 48)).isNull());

    MainWindow window;
    QVERIFY(!window.windowIcon().isNull());
}

QTEST_MAIN(TestAbout)
#include "tst_about.moc"
