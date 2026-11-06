// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Menu Pomoc → „O programie”: logo, nazwa, wersja, autor, strona www;
// ikona aplikacji we wszystkich rozmiarach z zasobów Qt.

#include "app/AppInfo.h"
#include "ui/AboutDialog.h"
#include "ui/HelpDialog.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QTextBrowser>
#include <QtTest>

using namespace cynamoncode;

class TestAbout : public QObject
{
    Q_OBJECT

private slots:
    void helpMenuHasAboutAction();
    void aboutDialogShowsProgramInfo();
    void applicationIconHasAllSizes();
    void helpDialogListsCurrentShortcuts();
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
    QCOMPARE(AppInfo::version(), QStringLiteral("1.0.0"));
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

void TestAbout::helpDialogListsCurrentShortcuts()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *helpAction = window.findChild<QAction *>(QStringLiteral("actionHelp"));
    QVERIFY(helpAction);
    QCOMPARE(helpAction->shortcut(), QKeySequence(Qt::Key_F1));
    helpAction->trigger();

    auto *dialog = window.findChild<HelpDialog *>(QStringLiteral("helpDialog"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    QVERIFY(!dialog->isModal()); // można czytać pomoc i pracować
    auto *browser = dialog->findChild<QTextBrowser *>(QStringLiteral("helpBrowser"));
    QVERIFY(browser);
    QVERIFY(!browser->openLinks());
    const QString text = browser->toPlainText();
    QVERIFY(text.contains(QStringLiteral("UTF-8")));
    QVERIFY(text.contains(QStringLiteral("Ctrl+N")));
    QVERIFY(text.contains(QStringLiteral("Zapisz jako")));
    QVERIFY(text.contains(AppInfo::version()));

    // Po zmianie skrótu pomoc pokazuje nowy.
    QList<ShortcutEntry> entries = window.shortcutEntries();
    for (ShortcutEntry &entry : entries) {
        if (entry.id == QLatin1String("actionGoToLine"))
            entry.current = {QKeySequence(Qt::ALT | Qt::Key_F7)};
    }
    window.applyShortcuts(entries);
    helpAction->trigger();
    QVERIFY(browser->toPlainText().contains(QStringLiteral("Alt+F7")));
    QCOMPARE(window.findChildren<HelpDialog *>().size(), qsizetype(1));
}

QTEST_MAIN(TestAbout)
#include "tst_about.moc"
