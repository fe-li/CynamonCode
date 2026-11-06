// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Ustawienia w oknie głównym (D029; TESTY.md T20): przełączniki widoku,
// powiększenie, czcionka i motyw stosowane do wszystkich kart i zapamiętane
// po ponownym uruchomieniu.

#include "ScriptedPrompts.h"
#include "editor/EditorWidget.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace cynamoncode;
using testing::ScriptedPrompts;

namespace {

void trigger(MainWindow &window, const char *actionName)
{
    auto *action = window.findChild<QAction *>(QLatin1String(actionName));
    QVERIFY2(action, actionName);
    action->trigger();
}

bool isChecked(MainWindow &window, const char *actionName)
{
    auto *action = window.findChild<QAction *>(QLatin1String(actionName));
    return action && action->isChecked();
}

int zoomOf(EditorWidget *editor)
{
    return static_cast<int>(editor->SendScintilla(QsciScintillaBase::SCI_GETZOOM));
}

} // namespace

class TestSettingsWindow : public QObject
{
    Q_OBJECT

private slots:
    void settingsSurviveRestart();
    void settingsApplyToAllAndNewTabs();
    void editorZoomSyncsAllTabs();
};

void TestSettingsWindow::settingsSurviveRestart()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cynamoncode.conf"));
    {
        auto prompts = std::make_unique<ScriptedPrompts>();
        prompts->fontAnswer = QFont(QStringLiteral("DejaVu Sans Mono"), 15);
        MainWindow window(std::move(prompts),
                          std::make_unique<QSettings>(path, QSettings::IniFormat));
        trigger(window, "actionThemeDark");
        trigger(window, "actionWordWrap");
        trigger(window, "actionShowWhitespace");
        trigger(window, "actionLineNumbers"); // wyłącz
        trigger(window, "actionZoomIn");
        trigger(window, "actionZoomIn");
        trigger(window, "actionFont");
    }

    MainWindow restarted(std::make_unique<ScriptedPrompts>(),
                         std::make_unique<QSettings>(path, QSettings::IniFormat));
    EditorWidget *editor = restarted.currentEditor();
    QVERIFY(restarted.theme() == ThemeId::Dark);
    QVERIFY(editor->theme().id == ThemeId::Dark);
    QVERIFY(editor->wrapMode() == QsciScintilla::WrapWord);
    QVERIFY(editor->whitespaceVisibility() == QsciScintilla::WsVisible);
    QVERIFY(!editor->lineNumbersVisible());
    QCOMPARE(zoomOf(editor), 2);
    QCOMPARE(editor->editorFont().pointSize(), 15);
    QCOMPARE(editor->editorFont().family(), QStringLiteral("DejaVu Sans Mono"));

    QVERIFY(isChecked(restarted, "actionThemeDark"));
    QVERIFY(isChecked(restarted, "actionWordWrap"));
    QVERIFY(isChecked(restarted, "actionShowWhitespace"));
    QVERIFY(!isChecked(restarted, "actionLineNumbers"));
    QVERIFY(!isChecked(restarted, "actionShowLineEndings"));
    QVERIFY(editor->autoCompletionSource() == QsciScintilla::AcsNone);
}

void TestSettingsWindow::settingsApplyToAllAndNewTabs()
{
    MainWindow window(std::make_unique<ScriptedPrompts>()); // bez pliku ustawień
    trigger(window, "actionNew");
    trigger(window, "actionWordWrap");
    trigger(window, "actionShowLineEndings");
    trigger(window, "actionZoomOut");
    trigger(window, "actionNew");

    QCOMPARE(window.documentCount(), 3);
    for (int i = 0; i < window.documentCount(); ++i) {
        EditorWidget *editor = window.editorAt(i);
        QVERIFY(editor->wrapMode() == QsciScintilla::WrapWord);
        QVERIFY(editor->eolVisibility());
        QCOMPARE(zoomOf(editor), -1);
        QVERIFY(editor->autoCompletionSource() == QsciScintilla::AcsNone);
    }
    trigger(window, "actionZoomReset");
    QCOMPARE(zoomOf(window.editorAt(0)), 0);
    QCOMPARE(window.editorSettings().zoom, 0);
}

void TestSettingsWindow::editorZoomSyncsAllTabs()
{
    MainWindow window(std::make_unique<ScriptedPrompts>());
    trigger(window, "actionNew");
    window.editorAt(0)->zoomIn(); // jak Ctrl+kółko w jednej karcie
    QCOMPARE(window.editorSettings().zoom, 1);
    QCOMPARE(zoomOf(window.editorAt(1)), 1);
}

QTEST_MAIN(TestSettingsWindow)
#include "tst_settings_window.moc"
