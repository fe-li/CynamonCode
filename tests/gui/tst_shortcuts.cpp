// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Konfigurowalne skróty klawiszowe (D045) i nawigacja klawiaturą (D046):
// reguły skrótów, okno „Skróty klawiszowe”, zapis różnic w QSettings,
// odczyt po ponownym uruchomieniu, przełączanie kart i widoków.

#include "TestHelpers.h"
#include "editor/EditorWidget.h"
#include "services/SettingsService.h"
#include "ui/MainWindow.h"
#include "ui/Shortcuts.h"
#include "ui/ShortcutsDialog.h"

#include <QAction>
#include <QApplication>
#include <QSet>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

using namespace cynamoncode;
using namespace cynamoncode::testing;

namespace {

QAction *actionOf(MainWindow &window, const char *name)
{
    return window.findChild<QAction *>(QLatin1String(name));
}

const ShortcutEntry *entryOf(const QList<ShortcutEntry> &entries, const QString &id)
{
    for (const ShortcutEntry &entry : entries) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

QKeySequence key(const char *text)
{
    return QKeySequence::fromString(QLatin1String(text), QKeySequence::PortableText);
}

} // namespace

class TestShortcuts : public QObject
{
    Q_OBJECT

private slots:
    void rulesForAllowedShortcuts();
    void plainMenuText();
    void storageSkipsInvalidSequences();
    void everyMenuCommandIsConfigurable();
    void defaultShortcutsAreUniqueAndAllowed();
    void dialogAssignMovesConflictingShortcut();
    void dialogRefusesPlainKeys();
    void dialogKeepsAlternateAndNormalizesBacktab();
    void everyCommandIdSurvivesSettings();
    void dialogClearResetAndFilter();
    void cancelLeavesShortcutsUnchanged();
    void acceptedShortcutsPersistAcrossRestart();
    void resetAllRemovesStoredOverrides();
    void storedConflictsAndGarbageAreResolved();
    void customShortcutWorksFromKeyboard();
    void shortcutWinsOverEditorKeyBinding();
    void ctrlTabSwitchesTabs();
    void f8FocusesOtherView();
};

void TestShortcuts::rulesForAllowedShortcuts()
{
    for (const char *allowed : {"Ctrl+K", "Alt+X", "Meta+E", "Ctrl+Shift+Tab", "F5", "Shift+F3",
                                "Ctrl+,"}) {
        QVERIFY2(Shortcuts::isAllowed(key(allowed)), allowed);
    }
    // Sekwencje kilku kombinacji: prefiks zasłaniałby inne skróty (np. Ctrl+S)
    // i połykał następny klawisz.
    for (const char *refused : {"A", "Shift+A", "Tab", "Esc", "Return", "Space", "Shift+Del",
                                "Ctrl+K, Ctrl+C", "Ctrl+S, Ctrl+A", "Ctrl+K, A"})
        QVERIFY2(!Shortcuts::isAllowed(key(refused)), refused);
    QVERIFY(!Shortcuts::isAllowed(QKeySequence()));

    // X11 zgłasza Ctrl+Shift+Tab jako Ctrl+Shift+Backtab.
    QCOMPARE(Shortcuts::normalized(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab)),
             key("Ctrl+Shift+Tab"));
    QCOMPARE(Shortcuts::normalized(QKeySequence(Qt::CTRL | Qt::Key_Backtab)),
             key("Ctrl+Shift+Tab"));
    QCOMPARE(Shortcuts::normalized(key("Ctrl+K")), key("Ctrl+K"));
}

void TestShortcuts::plainMenuText()
{
    QCOMPARE(Shortcuts::plainText(QStringLiteral("&Nowy")), QStringLiteral("Nowy"));
    QCOMPARE(Shortcuts::plainText(QStringLiteral("Zapisz &jako…")), QStringLiteral("Zapisz jako"));
    QCOMPARE(Shortcuts::plainText(QStringLiteral("A && B")), QStringLiteral("A & B"));
}

void TestShortcuts::storageSkipsInvalidSequences()
{
    const QList<QKeySequence> sequences = Shortcuts::fromStorage(
        {QStringLiteral("Ctrl+K"), QStringLiteral("A"), QStringLiteral("bzdura+++"),
         QStringLiteral("Ctrl+K"), QStringLiteral("F9")});
    QCOMPARE(sequences, (QList<QKeySequence>{key("Ctrl+K"), key("F9")}));
    QCOMPARE(Shortcuts::fromStorage(Shortcuts::toStorage(sequences)), sequences);
}

void TestShortcuts::everyMenuCommandIsConfigurable()
{
    MainWindow window;
    const QList<ShortcutEntry> entries = window.shortcutEntries();
    QSet<QString> ids;
    for (const ShortcutEntry &entry : entries) {
        QVERIFY2(!ids.contains(entry.id), qPrintable(entry.id));
        ids.insert(entry.id);
        QVERIFY(!entry.title.isEmpty());
        QVERIFY(!entry.title.contains(QLatin1Char('&')) || entry.title.contains(QStringLiteral(" & ")));
        QVERIFY(!entry.category.isEmpty());
        QCOMPARE(entry.current, entry.defaults);
    }
    for (const char *id : {"actionNew", "actionOpen", "actionSave", "actionQuit", "actionUndo",
                           "actionFind", "actionFindInAll", "actionGoToLine", "actionWordWrap",
                           "actionThemeDark", "actionToggleFileTree", "actionNextTab",
                           "actionPreviousTab", "actionFocusOtherView", "actionLanguage_cpp",
                           "actionShortcuts", "actionHelp", "actionAbout"}) {
        QVERIFY2(ids.contains(QLatin1String(id)), id);
    }
    const ShortcutEntry *dark = entryOf(entries, QStringLiteral("actionThemeDark"));
    QCOMPARE(dark->category, QStringLiteral("Widok → Motyw"));
    QCOMPARE(dark->title, QStringLiteral("Ciemny"));
    QCOMPARE(entryOf(entries, QStringLiteral("actionSaveAs"))->title, QStringLiteral("Zapisz jako"));
}

void TestShortcuts::defaultShortcutsAreUniqueAndAllowed()
{
    MainWindow window;
    QSet<QKeySequence> used;
    for (const ShortcutEntry &entry : window.shortcutEntries()) {
        for (const QKeySequence &sequence : entry.defaults) {
            QVERIFY2(Shortcuts::isAllowed(sequence), qPrintable(entry.id));
            QVERIFY2(!used.contains(sequence), qPrintable(sequence.toString()));
            used.insert(sequence);
        }
    }
    // Kluczowe skróty ze specyfikacji (§4).
    for (const char *expected : {"Ctrl+N", "Ctrl+O", "Ctrl+S", "Ctrl+Shift+S", "Ctrl+W",
                                 "Ctrl+Tab", "Ctrl+F", "Ctrl+H", "Ctrl+G", "Ctrl+Z", "Ctrl+Y",
                                 "Ctrl+A", "Ctrl++", "Ctrl+-", "F1"}) {
        QVERIFY2(used.contains(key(expected)), expected);
    }
    // Bez autouzupełniania — nic nie zajmuje Ctrl+Space.
    QVERIFY(!used.contains(key("Ctrl+Space")));
}

void TestShortcuts::dialogAssignMovesConflictingShortcut()
{
    MainWindow window;
    ShortcutsDialog dialog(window.shortcutEntries());

    QVERIFY(dialog.assign(QStringLiteral("actionNew"), key("Ctrl+K")));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionNew"))->current,
             QList<QKeySequence>{key("Ctrl+K")});

    // Ctrl+O należy do „Otwórz” — zostaje mu zabrany, z informacją.
    QVERIFY(dialog.assign(QStringLiteral("actionNew"), key("Ctrl+O")));
    QVERIFY(entryOf(dialog.entries(), QStringLiteral("actionOpen"))->current.isEmpty());
    QVERIFY2(dialog.message().contains(QStringLiteral("Otwórz")), qPrintable(dialog.message()));

    // Jedna z dwóch sekwencji „Ponów” — druga zostaje.
    QVERIFY(dialog.assign(QStringLiteral("actionFind"), key("Ctrl+Y")));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionRedo"))->current,
             QList<QKeySequence>{key("Ctrl+Shift+Z")});

    // Okno główne bez zmian, dopóki nie zatwierdzono.
    QCOMPARE(actionOf(window, "actionNew")->shortcut(), key("Ctrl+N"));
    QVERIFY(!dialog.assign(QStringLiteral("nieistniejace"), key("Ctrl+J")));
}

void TestShortcuts::dialogRefusesPlainKeys()
{
    MainWindow window;
    ShortcutsDialog dialog(window.shortcutEntries());
    QVERIFY(!dialog.assign(QStringLiteral("actionNew"), key("A")));
    QVERIFY(!dialog.assign(QStringLiteral("actionNew"), key("Tab")));
    QVERIFY(dialog.message().contains(QStringLiteral("Ctrl")));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionNew"))->current,
             QList<QKeySequence>{key("Ctrl+N")});
}

void TestShortcuts::dialogKeepsAlternateAndNormalizesBacktab()
{
    MainWindow window;
    ShortcutsDialog dialog(window.shortcutEntries());

    // „Przypisz” tego samego skrótu nie usuwa drugiego (Ctrl+PgDown).
    QVERIFY(!dialog.assign(QStringLiteral("actionNextTab"), key("Ctrl+Tab")));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionNextTab"))->current,
             (QList<QKeySequence>{key("Ctrl+Tab"), key("Ctrl+PgDown")}));

    // Ctrl+Shift+Backtab z X11 = Ctrl+Shift+Tab „Poprzedniej karty” — wykryta kolizja.
    QVERIFY(dialog.assign(QStringLiteral("actionNew"),
                          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab)));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionNew"))->current,
             QList<QKeySequence>{key("Ctrl+Shift+Tab")});
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionPreviousTab"))->current,
             QList<QKeySequence>{key("Ctrl+PgUp")});

    // Sekwencja kilku kombinacji odrzucona; Ctrl+S zostaje przy „Zapisz”.
    QVERIFY(!dialog.assign(QStringLiteral("actionSaveAll"), key("Ctrl+S, Ctrl+A")));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionSave"))->current,
             QList<QKeySequence>{key("Ctrl+S")});
}

void TestShortcuts::everyCommandIdSurvivesSettings()
{
    // Każdy identyfikator polecenia jest poprawnym kluczem ustawień (nic nie ginie po cichu).
    MainWindow window;
    QTemporaryDir dir;
    QSettings store(dir.filePath(QStringLiteral("ustawienia.conf")), QSettings::IniFormat);
    SettingsService service(store);
    QMap<QString, QStringList> overrides;
    for (const ShortcutEntry &entry : window.shortcutEntries())
        overrides.insert(entry.id, Shortcuts::toStorage(entry.defaults));
    QVERIFY(service.saveShortcutOverrides(overrides));
    QCOMPARE(service.shortcutOverrides(), overrides);
}

void TestShortcuts::dialogClearResetAndFilter()
{
    MainWindow window;
    ShortcutsDialog dialog(window.shortcutEntries());

    QVERIFY(dialog.clear(QStringLiteral("actionSave")));
    QVERIFY(entryOf(dialog.entries(), QStringLiteral("actionSave"))->current.isEmpty());
    QVERIFY(!dialog.clear(QStringLiteral("actionSave")));

    // Przywrócenie domyślnego zabiera sekwencję poleceniu, które ją przejęło.
    QVERIFY(dialog.assign(QStringLiteral("actionFind"), key("Ctrl+S")));
    QVERIFY(dialog.resetToDefault(QStringLiteral("actionSave")));
    QCOMPARE(entryOf(dialog.entries(), QStringLiteral("actionSave"))->current,
             QList<QKeySequence>{key("Ctrl+S")});
    QVERIFY(entryOf(dialog.entries(), QStringLiteral("actionFind"))->current.isEmpty());

    dialog.resetAll();
    for (const ShortcutEntry &entry : dialog.entries())
        QCOMPARE(entry.current, entry.defaults);

    // Filtr: ukryte wiersze nie dają się wybrać.
    dialog.setFilter(QStringLiteral("zapisz"));
    QVERIFY(dialog.selectCommand(QStringLiteral("actionSaveAs")));
    QCOMPARE(dialog.selectedId(), QStringLiteral("actionSaveAs"));
    QVERIFY(!dialog.selectCommand(QStringLiteral("actionOpen")));
    dialog.setFilter(QString());
    QVERIFY(dialog.selectCommand(QStringLiteral("actionOpen")));
}

void TestShortcuts::cancelLeavesShortcutsUnchanged()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cynamoncode.conf"));
    WindowFixture fixture(path);
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    trigger(window, "actionShortcuts");
    auto *dialog = window.findChild<ShortcutsDialog *>(QStringLiteral("shortcutsDialog"));
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());
    QVERIFY(dialog->assign(QStringLiteral("actionNew"), key("Ctrl+K")));
    dialog->reject();

    QCOMPARE(actionOf(window, "actionNew")->shortcut(), key("Ctrl+N"));
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("shortcuts"));
    QVERIFY(settings.childKeys().isEmpty());
}

void TestShortcuts::acceptedShortcutsPersistAcrossRestart()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cynamoncode.conf"));
    {
        WindowFixture fixture(path);
        MainWindow &window = *fixture.window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        trigger(window, "actionShortcuts");
        auto *dialog = window.findChild<ShortcutsDialog *>(QStringLiteral("shortcutsDialog"));
        QVERIFY(dialog);
        QVERIFY(dialog->assign(QStringLiteral("actionNew"), key("Ctrl+O"))); // zabrane „Otwórz”
        QVERIFY(dialog->assign(QStringLiteral("actionGoToLine"), key("Alt+F7")));
        dialog->accept();

        QCOMPARE(actionOf(window, "actionNew")->shortcuts(), QList<QKeySequence>{key("Ctrl+O")});
        QVERIFY(actionOf(window, "actionOpen")->shortcuts().isEmpty());
        QCOMPARE(actionOf(window, "actionGoToLine")->shortcut(), key("Alt+F7"));
    }
    {
        // Zapisane są tylko różnice względem domyślnych.
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("shortcuts"));
        QStringList keys = settings.childKeys();
        keys.sort();
        QCOMPARE(keys, (QStringList{QStringLiteral("actionGoToLine"), QStringLiteral("actionNew"),
                                    QStringLiteral("actionOpen")}));
        QCOMPARE(settings.value(QStringLiteral("actionOpen")).toString(), QString());
    }
    WindowFixture restarted(path);
    MainWindow &window = *restarted.window;
    QCOMPARE(actionOf(window, "actionNew")->shortcuts(), QList<QKeySequence>{key("Ctrl+O")});
    QVERIFY(actionOf(window, "actionOpen")->shortcuts().isEmpty());
    QCOMPARE(actionOf(window, "actionGoToLine")->shortcut(), key("Alt+F7"));
    // Domyślne nadal widoczne dla „Przywróć domyślny”.
    QCOMPARE(entryOf(window.shortcutEntries(), QStringLiteral("actionNew"))->defaults,
             QList<QKeySequence>{key("Ctrl+N")});
}

void TestShortcuts::resetAllRemovesStoredOverrides()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cynamoncode.conf"));
    WindowFixture fixture(path);
    MainWindow &window = *fixture.window;

    QList<ShortcutEntry> entries = window.shortcutEntries();
    for (ShortcutEntry &entry : entries) {
        if (entry.id == QLatin1String("actionFind"))
            entry.current = {key("Alt+F")};
    }
    window.applyShortcuts(entries);
    QCOMPARE(actionOf(window, "actionFind")->shortcut(), key("Alt+F"));

    ShortcutsDialog *dialog = window.openShortcutsDialog();
    dialog->resetAll();
    dialog->accept();
    QCOMPARE(actionOf(window, "actionFind")->shortcut(), key("Ctrl+F"));
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("shortcuts"));
    QVERIFY(settings.childKeys().isEmpty());
}

void TestShortcuts::storedConflictsAndGarbageAreResolved()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("cynamoncode.conf"));
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("shortcuts"));
        settings.setValue(QStringLiteral("actionOpen"), QStringLiteral("Ctrl+N")); // zajęty przez „Nowy”
        settings.setValue(QStringLiteral("actionSave"), QStringLiteral("bzdura+++"));
        settings.setValue(QStringLiteral("actionCut"), QStringLiteral("X")); // bez modyfikatora
        settings.setValue(QStringLiteral("actionNieznana"), QStringLiteral("Ctrl+J"));
    }
    WindowFixture fixture(path);
    MainWindow &window = *fixture.window;
    // Zmiana użytkownika wygrywa z domyślnym skrótem innego polecenia.
    QCOMPARE(actionOf(window, "actionOpen")->shortcuts(), QList<QKeySequence>{key("Ctrl+N")});
    QVERIFY(actionOf(window, "actionNew")->shortcuts().isEmpty());
    // Błędne wpisy są pomijane — zostaje skrót domyślny (D045).
    QCOMPARE(actionOf(window, "actionSave")->shortcuts(), QList<QKeySequence>{key("Ctrl+S")});
    QCOMPARE(actionOf(window, "actionCut")->shortcuts(), QList<QKeySequence>{key("Ctrl+X")});
    QSet<QKeySequence> used;
    for (const ShortcutEntry &entry : window.shortcutEntries()) {
        for (const QKeySequence &sequence : entry.current) {
            QVERIFY2(!used.contains(sequence), qPrintable(sequence.toString()));
            used.insert(sequence);
        }
    }
}

void TestShortcuts::customShortcutWorksFromKeyboard()
{
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    QList<ShortcutEntry> entries = window.shortcutEntries();
    for (ShortcutEntry &entry : entries) {
        if (entry.id == QLatin1String("actionNew"))
            entry.current = {key("Ctrl+K")};
    }
    window.applyShortcuts(entries);

    EditorWidget *editor = window.currentEditor();
    editor->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget *>(editor));
    QCOMPARE(window.documentCount(), 1);
    QTest::keyClick(editor, Qt::Key_K, Qt::ControlModifier);
    QCOMPARE(window.documentCount(), 2);
    // Stary skrót nie działa — Ctrl+N nie tworzy dokumentu ani nie wpisuje tekstu.
    QTest::keyClick(window.currentEditor(), Qt::Key_N, Qt::ControlModifier);
    QCOMPARE(window.documentCount(), 2);
    QVERIFY(window.currentEditor()->text().isEmpty());
}

void TestShortcuts::shortcutWinsOverEditorKeyBinding()
{
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    // Ctrl+T i Ctrl+L to w QScintilli zamiana i wycięcie wiersza.
    QList<ShortcutEntry> entries = window.shortcutEntries();
    for (ShortcutEntry &entry : entries) {
        if (entry.id == QLatin1String("actionNew"))
            entry.current = {key("Ctrl+T")};
        if (entry.id == QLatin1String("actionGoToLine"))
            entry.current = {key("Ctrl+L")};
    }
    window.applyShortcuts(entries);

    EditorWidget *editor = window.currentEditor();
    edit(editor, QStringLiteral("a\nb"));
    editor->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget *>(editor));
    QTest::keyClick(editor, Qt::Key_T, Qt::ControlModifier);
    QCOMPARE(window.documentCount(), 2);
    QCOMPARE(editor->text(), QStringLiteral("a\nb"));

    // Nowa karta (utworzona po zmianie) też oddaje klawisz oknu.
    EditorWidget *second = window.currentEditor();
    edit(second, QStringLiteral("x\ny"));
    fixture.prompts->lineNumber = 1; // „Przejdź do wiersza” — wiersz 1
    QTest::keyClick(second, Qt::Key_L, Qt::ControlModifier);
    QCOMPARE(second->text(), QStringLiteral("x\ny"));
    int line = -1;
    int column = -1;
    second->getCursorPosition(&line, &column);
    QCOMPARE(line, 0);

    // Po przywróceniu domyślnych klawisz znów należy do edytora.
    QList<ShortcutEntry> defaults = window.shortcutEntries();
    for (ShortcutEntry &entry : defaults)
        entry.current = entry.defaults;
    window.applyShortcuts(defaults);
    second->setCursorPosition(1, 1);
    QTest::keyClick(second, Qt::Key_T, Qt::ControlModifier);
    QCOMPARE(window.documentCount(), 2);
    QCOMPARE(second->text(), QStringLiteral("y\nx")); // zamiana wierszy QScintilli
}

void TestShortcuts::ctrlTabSwitchesTabs()
{
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    trigger(window, "actionNew");
    trigger(window, "actionNew");
    QTabWidget *tabs = window.pane(0);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->currentIndex(), 2);

    QTest::keyClick(window.currentEditor(), Qt::Key_Tab, Qt::ControlModifier);
    QCOMPARE(tabs->currentIndex(), 0); // zawija
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(window.currentEditor()));
    QVERIFY(window.currentEditor()->text().isEmpty()); // bez wstawionego tabulatora

    QTest::keyClick(window.currentEditor(), Qt::Key_Backtab,
                    Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(tabs->currentIndex(), 2);
    QTest::keyClick(window.currentEditor(), Qt::Key_PageUp, Qt::ControlModifier);
    QCOMPARE(tabs->currentIndex(), 1);
    QTest::keyClick(window.currentEditor(), Qt::Key_PageDown, Qt::ControlModifier);
    QCOMPARE(tabs->currentIndex(), 2);
}

void TestShortcuts::f8FocusesOtherView()
{
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    // Jeden widok: F8 nic nie psuje.
    QVERIFY(!window.focusOtherPane());
    QCOMPARE(window.activePane(), 0);

    QVERIFY(window.cloneCurrentToOtherView());
    QCOMPARE(window.activePane(), 1);
    QTest::keyClick(window.currentEditor(), Qt::Key_F8);
    QCOMPARE(window.activePane(), 0);
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget *>(window.pane(0)->currentWidget()));
    QTest::keyClick(window.currentEditor(), Qt::Key_F8);
    QCOMPARE(window.activePane(), 1);
}

QTEST_MAIN(TestShortcuts)
#include "tst_shortcuts.moc"
