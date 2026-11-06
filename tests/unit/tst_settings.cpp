// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// SettingsService: wartości domyślne, zapis i odczyt, odporność na błędne
// wartości w pliku konfiguracyjnym (D029).

#include "services/SettingsService.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace cynamoncode;

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenEmpty();
    void roundTrip();
    void invalidValuesFallBackToDefaults();
    void windowGeometryRoundTrip();
    void shortcutOverridesRoundTrip();
    void shortcutOverridesSurviveRestart();
    void invalidShortcutEntriesSkipped();
};

void TestSettings::defaultsWhenEmpty()
{
    QTemporaryDir dir;
    QSettings store(dir.filePath(QStringLiteral("ustawienia.conf")), QSettings::IniFormat);
    const EditorSettings loaded = SettingsService(store).load();
    QVERIFY(loaded == EditorSettings{});
    QVERIFY(loaded.lineNumbers);
    QCOMPARE(loaded.theme, QStringLiteral("light"));
}

void TestSettings::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("ustawienia.conf"));
    EditorSettings saved;
    saved.fontFamily = QStringLiteral("DejaVu Sans Mono");
    saved.fontPointSize = 14;
    saved.wordWrap = true;
    saved.lineNumbers = false;
    saved.showWhitespace = true;
    saved.showLineEndings = true;
    saved.zoom = -3;
    saved.theme = QStringLiteral("dark");
    {
        QSettings store(path, QSettings::IniFormat);
        SettingsService(store).save(saved);
    }
    QSettings reopened(path, QSettings::IniFormat);
    QVERIFY(SettingsService(reopened).load() == saved);
}

void TestSettings::invalidValuesFallBackToDefaults()
{
    QTemporaryDir dir;
    QSettings store(dir.filePath(QStringLiteral("ustawienia.conf")), QSettings::IniFormat);
    store.setValue(QStringLiteral("editor/zoom"), 999);
    store.setValue(QStringLiteral("editor/fontSize"), QStringLiteral("duży"));
    store.setValue(QStringLiteral("editor/wordWrap"), QStringLiteral("tak"));
    store.setValue(QStringLiteral("editor/lineNumbers"), QStringLiteral("0"));
    store.setValue(QStringLiteral("appearance/theme"), QStringLiteral("neon"));

    const EditorSettings loaded = SettingsService(store).load();
    QCOMPARE(loaded.zoom, SettingsService::MaxZoom);
    QCOMPARE(loaded.fontPointSize, 0);
    QCOMPARE(loaded.wordWrap, false);
    QCOMPARE(loaded.lineNumbers, false);
    QCOMPARE(loaded.theme, QStringLiteral("light"));
}

void TestSettings::windowGeometryRoundTrip()
{
    QTemporaryDir dir;
    QSettings store(dir.filePath(QStringLiteral("ustawienia.conf")), QSettings::IniFormat);
    SettingsService service(store);
    QVERIFY(service.windowGeometry().isEmpty());
    service.saveWindowGeometry(QByteArray("\x01\x02geometria", 11));
    QCOMPARE(service.windowGeometry(), QByteArray("\x01\x02geometria", 11));
}

void TestSettings::shortcutOverridesRoundTrip()
{
    QTemporaryDir dir;
    QSettings store(dir.filePath(QStringLiteral("ustawienia.conf")), QSettings::IniFormat);
    SettingsService service(store);
    QVERIFY(service.shortcutOverrides().isEmpty());

    // Przecinek i średnik w sekwencji, kilka sekwencji, polecenie bez skrótu (D045).
    QMap<QString, QStringList> overrides;
    overrides.insert(QStringLiteral("actionNew"), {QStringLiteral("Ctrl+,")});
    overrides.insert(QStringLiteral("actionRedo"),
                     {QStringLiteral("Ctrl+;"), QStringLiteral("Ctrl+Shift+Z")});
    overrides.insert(QStringLiteral("actionQuit"), {});
    service.saveShortcutOverrides(overrides);
    QCOMPARE(service.shortcutOverrides(), overrides);

    // Kolejny zapis zastępuje całą grupę (przywrócone domyślne znikają).
    overrides.remove(QStringLiteral("actionRedo"));
    service.saveShortcutOverrides(overrides);
    QCOMPARE(service.shortcutOverrides(), overrides);
    service.saveShortcutOverrides({});
    QVERIFY(service.shortcutOverrides().isEmpty());
}

void TestSettings::shortcutOverridesSurviveRestart()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("ustawienia.conf"));
    QMap<QString, QStringList> overrides;
    overrides.insert(QStringLiteral("actionFind"), {QStringLiteral("Alt+F7")});
    overrides.insert(QStringLiteral("actionSave"), {});
    {
        QSettings store(path, QSettings::IniFormat);
        SettingsService(store).saveShortcutOverrides(overrides);
    }
    QSettings store(path, QSettings::IniFormat);
    QCOMPARE(SettingsService(store).shortcutOverrides(), overrides);
}

void TestSettings::invalidShortcutEntriesSkipped()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("ustawienia.conf"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // Ręcznie edytowany plik: lista zamiast tekstu, niedozwolony identyfikator.
        file.write("[shortcuts]\n"
                   "actionOpen=Ctrl+P\n"
                   "actionFind=Ctrl+1, Ctrl+2\n"
                   "bad-id=Ctrl+B\n");
    }
    QSettings store(path, QSettings::IniFormat);
    const QMap<QString, QStringList> overrides = SettingsService(store).shortcutOverrides();
    QCOMPARE(overrides.size(), 1);
    QCOMPARE(overrides.value(QStringLiteral("actionOpen")), QStringList{QStringLiteral("Ctrl+P")});
}

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
