// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Pliki Windows-1250 w oknie (D051): pytanie przy otwarciu pliku spoza UTF-8,
// zapis w tym samym kodowaniu, znak spoza kodowania → pytanie o UTF-8,
// „Zapisz w kodowaniu UTF-8”, kodowanie w pasku stanu i w sesji.

#include "TestHelpers.h"
#include "editor/EditorWidget.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QLabel>
#include <QTemporaryDir>
#include <QtTest>

using namespace cynamoncode;
using namespace cynamoncode::testing;

namespace {

// „Zażółć gęślą jaźń” + CRLF w Windows-1250.
const QByteArray Cp1250Text("Za\xBF\xF3\xB3\xE6 g\xEA\x9Cl\xB9 ja\x9F\xF1\r\n");

QString encodingLabel(MainWindow &window)
{
    auto *label = window.findChild<QLabel *>(QStringLiteral("encodingLabel"));
    return label ? label->text() : QString();
}

bool actionEnabled(MainWindow &window, const char *name)
{
    auto *action = window.findChild<QAction *>(QLatin1String(name));
    return action && action->isEnabled();
}

} // namespace

class TestWindows1250 : public QObject
{
    Q_OBJECT

private slots:
    void declinedQuestionLeavesFileClosed();
    void openEditAndSaveKeepsWindows1250();
    void unencodableCharacterAsksBeforeSaving();
    void saveAsUtf8ActionConvertsFile();
    void sessionRestoresEncodingWithoutQuestion();
};

void TestWindows1250::declinedQuestionLeavesFileClosed()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("stary.txt"));
    QVERIFY(writeBytes(path, Cp1250Text));
    WindowFixture fixture;
    MainWindow &window = *fixture.window;

    QVERIFY(!window.openFile(path));
    QCOMPARE(fixture.prompts->windows1250Questions, 1);
    QCOMPARE(fixture.prompts->errors.size(), 1);
    QVERIFY(fixture.prompts->errors.first().contains(QStringLiteral("Windows-1250")));
    QCOMPARE(window.documentCount(), 1); // tylko pusta karta „Nowy”
    QCOMPARE(readBytes(path), Cp1250Text);

    // Plik UTF-8 nie wywołuje pytania.
    const QString utf8 = dir.filePath(QStringLiteral("nowy.txt"));
    QVERIFY(writeBytes(utf8, QStringLiteral("zażółć\n").toUtf8()));
    QVERIFY(window.openFile(utf8));
    QCOMPARE(fixture.prompts->windows1250Questions, 1);
    QCOMPARE(encodingLabel(window), QStringLiteral("UTF-8"));
    QVERIFY(!actionEnabled(window, "actionSaveAsUtf8"));
}

void TestWindows1250::openEditAndSaveKeepsWindows1250()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("stary.txt"));
    QVERIFY(writeBytes(path, Cp1250Text));
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    fixture.prompts->openAsWindows1250Answer = true;

    QVERIFY(window.openFile(path));
    EditorWidget *editor = window.currentEditor();
    QCOMPARE(editor->text(), QStringLiteral("Zażółć gęślą jaźń\r\n"));
    QCOMPARE(encodingLabel(window), QStringLiteral("Windows-1250"));
    QVERIFY(actionEnabled(window, "actionSaveAsUtf8"));

    edit(editor, editor->text() + QStringLiteral("Łódź\r\n"));
    trigger(window, "actionSave");
    QCOMPARE(readBytes(path), Cp1250Text + QByteArray("\xA3\xF3" "d\x9F\r\n"));
    QVERIFY(!editor->isModified());
    QCOMPARE(fixture.prompts->saveAsUtf8Questions, 0);
}

void TestWindows1250::unencodableCharacterAsksBeforeSaving()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("stary.txt"));
    QVERIFY(writeBytes(path, Cp1250Text));
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    fixture.prompts->openAsWindows1250Answer = true;
    QVERIFY(window.openFile(path));
    EditorWidget *editor = window.currentEditor();

    const QString withEmoji = QStringLiteral("Kot 🐈\r\n");
    edit(editor, withEmoji);

    // „Anuluj”: nic nie zapisano, dokument nadal zmieniony i w Windows-1250.
    trigger(window, "actionSave");
    QCOMPARE(fixture.prompts->saveAsUtf8Questions, 1);
    QCOMPARE(fixture.prompts->lastUnencodableCharacter, QStringLiteral("🐈"));
    QCOMPARE(readBytes(path), Cp1250Text);
    QVERIFY(editor->isModified());
    QCOMPARE(encodingLabel(window), QStringLiteral("Windows-1250"));

    // „Zapisz jako UTF-8”.
    fixture.prompts->saveAsUtf8Answer = true;
    trigger(window, "actionSave");
    QVERIFY2(fixture.prompts->errors.isEmpty(), qPrintable(fixture.prompts->errors.join(u'\n')));
    QCOMPARE(fixture.prompts->saveAsUtf8Questions, 2);
    QCOMPARE(readBytes(path), withEmoji.toUtf8());
    QVERIFY(!editor->isModified());
    QCOMPARE(encodingLabel(window), QStringLiteral("UTF-8"));
    QVERIFY(!actionEnabled(window, "actionSaveAsUtf8"));
}

void TestWindows1250::saveAsUtf8ActionConvertsFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("stary.txt"));
    QVERIFY(writeBytes(path, Cp1250Text));
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    fixture.prompts->openAsWindows1250Answer = true;
    QVERIFY(window.openFile(path));

    trigger(window, "actionSaveAsUtf8");
    QVERIFY2(fixture.prompts->errors.isEmpty(), qPrintable(fixture.prompts->errors.join(u'\n')));
    QCOMPARE(readBytes(path), QStringLiteral("Zażółć gęślą jaźń\r\n").toUtf8());
    QCOMPARE(encodingLabel(window), QStringLiteral("UTF-8"));
    QVERIFY(!actionEnabled(window, "actionSaveAsUtf8"));
    QVERIFY(fixture.prompts->errors.isEmpty());
}

void TestWindows1250::sessionRestoresEncodingWithoutQuestion()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("stary.txt"));
    const QString settings = dir.filePath(QStringLiteral("cynamoncode.conf"));
    QVERIFY(writeBytes(path, Cp1250Text));
    {
        WindowFixture fixture(settings);
        fixture.prompts->openAsWindows1250Answer = true;
        QVERIFY(fixture.window->openFile(path));
        QVERIFY(fixture.window->saveSession());
    }
    WindowFixture restored(settings);
    QVERIFY(restored.window->restoreSession());
    QCOMPARE(restored.prompts->windows1250Questions, 0);
    QCOMPARE(restored.window->currentEditor()->text(), QStringLiteral("Zażółć gęślą jaźń\r\n"));
    QCOMPARE(encodingLabel(*restored.window), QStringLiteral("Windows-1250"));
}

QTEST_MAIN(TestWindows1250)
#include "tst_windows1250.moc"
