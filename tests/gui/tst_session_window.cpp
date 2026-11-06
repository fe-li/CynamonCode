// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Przywracanie sesji w oknie głównym (D034; TESTY.md T21): kolejność kart
// w obu panelach, klon, aktywna karta, kursor, ręczny język, folder; brakujące
// pliki pomijane z informacją; bez treści dokumentów w pliku sesji.

#include "TestHelpers.h"
#include "ui/FileTreeDock.h"

#include <QDir>
#include <QElapsedTimer>
#include <QTabWidget>
#include <QTemporaryDir>

using namespace cynamoncode;
using namespace cynamoncode::testing;

class TestSessionWindow : public QObject
{
    Q_OBJECT

private slots:
    void sessionSurvivesRestart();
    void missingFilesAreSkipped();
    void restoreCanBeDisabled();
    void windowWithoutSettingsHasNoSession();
    void firstRunCreatesSettingsFolder();
    void corruptPositionsAreClamped();
};

void TestSessionWindow::firstRunCreatesSettingsFolder()
{
    // Regresja przeglądu E4 (D5): katalog ustawień jeszcze nie istnieje.
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("nowy/katalog/cynamoncode.conf"));
    const QString file = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(file, "a"));
    {
        WindowFixture f(settings);
        f.window->show();
        QVERIFY(f.window->openFile(file));
        QVERIFY(f.window->close());
    }
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("nowy/katalog/session.json"))));
}

void TestSessionWindow::corruptPositionsAreClamped()
{
    // Regresja przeglądu E4 (D6): ogromna kolumna z uszkodzonej sesji.
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("cynamoncode.conf"));
    const QString file = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(file, "krótka\ndruga linia\n"));
    const QByteArray json = QByteArray("{\"version\":1,\"panes\":[{\"current\":0,\"tabs\":[{\"path\":")
                            + "\"" + file.toUtf8() + "\""
                            + ",\"line\":999999,\"index\":2147483647,\"firstVisibleLine\":999999}]},"
                              "{\"current\":-1,\"tabs\":[]}],\"activePane\":0}";
    QVERIFY(writeBytes(dir.filePath(QStringLiteral("session.json")), json));

    WindowFixture f(settings);
    QElapsedTimer timer;
    timer.start();
    QVERIFY(f.window->restoreSession());
    QVERIFY2(timer.elapsed() < 2000, qPrintable(QString::number(timer.elapsed())));
    int line = -1;
    int index = -1;
    f.window->currentEditor()->getCursorPosition(&line, &index);
    QCOMPARE(line, 2); // ostatnia (pusta) linia
    QCOMPARE(index, 0);
}

void TestSessionWindow::sessionSurvivesRestart()
{
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("cynamoncode.conf"));
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("projekt")));
    const QString a = dir.filePath(QStringLiteral("projekt/a.cpp"));
    const QString b = dir.filePath(QStringLiteral("projekt/b.txt"));
    const QString c = dir.filePath(QStringLiteral("projekt/c.py"));
    QVERIFY(writeBytes(a, "int a;\nint b;\nint c;\n"));
    QVERIFY(writeBytes(b, "TAJNA TREŚĆ 1\nlinia 2\n"));
    QVERIFY(writeBytes(c, "x = 1\n"));

    {
        WindowFixture f(settings);
        f.window->show();
        QVERIFY(f.window->openFile(a));
        QVERIFY(f.window->openFile(b));
        f.window->currentEditor()->setCursorPosition(1, 3);
        trigger(*f.window, "actionLanguage_sql"); // ręczny język
        QVERIFY(f.window->cloneCurrentToOtherView()); // b także w panelu 2
        QVERIFY(f.window->openFile(c, 1));
        trigger(*f.window, "actionNew"); // bez pliku — nie trafia do sesji
        f.prompts->unsavedAnswers = {};
        f.window->openFolder(dir.filePath(QStringLiteral("projekt")));
        f.window->openFile(a); // aktywny: panel 0, a.cpp
        QVERIFY(f.window->close());
    }
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("session.json"))));
    const QByteArray json = readBytes(dir.filePath(QStringLiteral("session.json")));
    QVERIFY(!json.contains("TAJNA"));

    WindowFixture f(settings);
    QVERIFY(f.window->restoreSession());
    QTabWidget *left = f.window->pane(0);
    QTabWidget *right = f.window->pane(1);
    QCOMPARE(left->count(), 2);
    QCOMPARE(left->tabText(0), QStringLiteral("a.cpp"));
    QCOMPARE(left->tabText(1), QStringLiteral("b.txt"));
    QCOMPARE(right->count(), 3 - 1); // klon b.txt i c.py (Nowy 2 pominięty)
    QCOMPARE(right->tabText(0), QStringLiteral("b.txt"));
    QCOMPARE(right->tabText(1), QStringLiteral("c.py"));
    QCOMPARE(f.window->activePane(), 0);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("int a;\nint b;\nint c;\n"));

    // b.txt: ręczny język SQL, kursor, wspólny bufor obu widoków.
    auto *bLeft = qobject_cast<EditorWidget *>(left->widget(1));
    auto *bRight = qobject_cast<EditorWidget *>(right->widget(0));
    QCOMPARE(int(bLeft->language()), int(LanguageId::Sql));
    QCOMPARE(int(bRight->language()), int(LanguageId::Sql));
    int line = -1;
    int index = -1;
    bLeft->getCursorPosition(&line, &index);
    QCOMPARE(line, 1);
    QCOMPARE(index, 3);
    edit(bRight, QStringLiteral("edycja w klonie"));
    QCOMPARE(bLeft->text(), QStringLiteral("edycja w klonie"));

    QCOMPARE(f.window->workspaceFolder(), QDir(dir.filePath(QStringLiteral("projekt"))).absolutePath());
    QVERIFY(!f.window->fileTree()->isHidden());
    QVERIFY(f.prompts->infos.isEmpty());
    QVERIFY(f.prompts->errors.isEmpty());
}

void TestSessionWindow::missingFilesAreSkipped()
{
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("cynamoncode.conf"));
    const QString keep = dir.filePath(QStringLiteral("zostaje.txt"));
    const QString gone = dir.filePath(QStringLiteral("zniknie.txt"));
    QVERIFY(writeBytes(keep, "1"));
    QVERIFY(writeBytes(gone, "2"));
    {
        WindowFixture f(settings);
        f.window->show();
        QVERIFY(f.window->openFile(keep));
        QVERIFY(f.window->openFile(gone));
        QVERIFY(f.window->close());
    }
    QVERIFY(QFile::remove(gone));

    WindowFixture f(settings);
    QVERIFY(f.window->restoreSession());
    QCOMPARE(f.window->documentCount(), 1);
    QCOMPARE(f.window->currentEditor()->text(), QStringLiteral("1"));
    QCOMPARE(f.prompts->infos.size(), 1);
    QVERIFY(f.prompts->infos.first().contains(QStringLiteral("zniknie.txt")));
}

void TestSessionWindow::restoreCanBeDisabled()
{
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("cynamoncode.conf"));
    const QString file = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(file, "a"));
    {
        WindowFixture f(settings);
        f.window->show();
        QVERIFY(f.window->openFile(file));
        QVERIFY(f.window->saveSession());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("session.json"))));
        trigger(*f.window, "actionRestoreSession"); // wyłącz
        QVERIFY(!f.window->editorSettings().restoreSession);
        // Wyłączenie usuwa zapamiętane ścieżki; zamknięcie niczego nie zapisuje (D034).
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("session.json"))));
        QVERIFY(f.window->close());
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("session.json"))));
    }
    WindowFixture f(settings);
    QVERIFY(!f.window->restoreSession());
    QCOMPARE(f.window->documentCount(), 1); // tylko pusty „Nowy 1”
    QVERIFY(f.window->currentEditor()->text().isEmpty());
}

void TestSessionWindow::windowWithoutSettingsHasNoSession()
{
    WindowFixture f;
    QVERIFY(f.window->sessionFilePath().isEmpty());
    QVERIFY(!f.window->restoreSession());
    QVERIFY(!f.window->saveSession());
}

QTEST_MAIN(TestSessionWindow)
#include "tst_session_window.moc"
