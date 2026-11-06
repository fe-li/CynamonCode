// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Ustawienia → „Domyślny program dla plików…” (D047): wybór typów w oknie, zapis
// w mimeapps.list użytkownika dopiero po „OK”, przywrócenie poprzedniego programu,
// zgodność listy typów z wpisem .desktop. Ścieżki XDG wskazują katalog tymczasowy.

#include "TestHelpers.h"
#include "services/MimeAppsService.h"
#include "ui/DefaultAppsDialog.h"
#include "ui/MainWindow.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTemporaryDir>
#include <QtTest>

using namespace cynamoncode;
using namespace cynamoncode::testing;

namespace {

const QString Id = QStringLiteral("pl.feliszewski.CynamonCode.desktop");

QString readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QStringList desktopMimeTypes()
{
    const QString text = readText(QStringLiteral(CYNAMONCODE_SOURCE_DIR
                                                 "/packaging/linux/pl.feliszewski.CynamonCode.desktop"));
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("MimeType=")))
            return line.mid(9).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    }
    return {};
}

} // namespace

class TestDefaultApps : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void typeGroupsMatchDesktopEntry();
    void actionInSettingsMenu();
    void acceptWritesUserMimeApps();
    void uncheckRestoresPreviousProgram();
    void cancelChangesNothing();
    void partialGroupIsShownAndCompleted();
    void notInstalledAllowsOnlyRemoval();

private:
    DefaultAppsDialog *openDialog(MainWindow &window);
    QString mimeAppsPath() const { return m_config.filePath(QStringLiteral("mimeapps.list")); }

    QTemporaryDir m_root;
    QDir m_config;
    QString m_desktopEntry;
};

void TestDefaultApps::initTestCase()
{
    QVERIFY(m_root.isValid());
    const QDir root(m_root.path());
    QVERIFY(root.mkpath(QStringLiteral("config")));
    QVERIFY(root.mkpath(QStringLiteral("data/applications")));
    QVERIFY(root.mkpath(QStringLiteral("system")));
    m_config = QDir(root.filePath(QStringLiteral("config")));
    m_desktopEntry = root.filePath(QStringLiteral("data/applications/") + Id);
    qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    qputenv("XDG_DATA_HOME", root.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_DATA_DIRS", root.filePath(QStringLiteral("system")).toUtf8());
    qputenv("XDG_CURRENT_DESKTOP", "X-Cinnamon");
}

void TestDefaultApps::init()
{
    QFile::remove(mimeAppsPath());
    QFile::remove(m_config.filePath(QStringLiteral("x-cinnamon-mimeapps.list")));
    QFile entry(m_desktopEntry);
    QVERIFY(entry.open(QIODevice::WriteOnly)); // „zainstalowany” wpis menu
}

DefaultAppsDialog *TestDefaultApps::openDialog(MainWindow &window)
{
    // Bez QVERIFY — makra Qt Test wymagają funkcji void; wynik sprawdza test.
    auto *action = window.findChild<QAction *>(QStringLiteral("actionDefaultApps"));
    if (!action)
        return nullptr;
    action->trigger();
    // Nowe okno przy każdym otwarciu (poprzednie usuwa deleteLater) — najmłodsze
    // dziecko jest ostatnie na liście.
    const QList<DefaultAppsDialog *> dialogs =
        window.findChildren<DefaultAppsDialog *>(QStringLiteral("defaultAppsDialog"));
    DefaultAppsDialog *dialog = dialogs.isEmpty() ? nullptr : dialogs.constLast();
    if (dialog && !QTest::qWaitFor([dialog] { return dialog->isVisible(); }, 2000))
        return nullptr;
    return dialog;
}

void TestDefaultApps::typeGroupsMatchDesktopEntry()
{
    // Każdy typ z okna jest zadeklarowany we wpisie .desktop i odwrotnie — inaczej
    // system nie zaproponowałby programu albo okno pomijałoby typ.
    const QStringList declared = desktopMimeTypes();
    QVERIFY(!declared.isEmpty());
    QStringList offered;
    for (const FileTypeGroup &group : DefaultAppsDialog::fileTypeGroups())
        offered += group.mimeTypes;
    QStringList sortedDeclared = declared;
    sortedDeclared.sort();
    offered.sort();
    QCOMPARE(offered, sortedDeclared);
}

void TestDefaultApps::actionInSettingsMenu()
{
    MainWindow window;
    auto *action = window.findChild<QAction *>(QStringLiteral("actionDefaultApps"));
    QVERIFY(action);
    bool listed = false;
    for (const ShortcutEntry &entry : window.shortcutEntries())
        listed = listed || (entry.id == QLatin1String("actionDefaultApps")
                            && entry.category == QLatin1String("Ustawienia"));
    QVERIFY(listed);
}

void TestDefaultApps::acceptWritesUserMimeApps()
{
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    DefaultAppsDialog *dialog = openDialog(window);
    QVERIFY(dialog);
    for (const QString &group : dialog->groupNames())
        QCOMPARE(dialog->checkState(group), Qt::Unchecked); // nic samoczynnie
    for (const char *group : {"HTML", "CSS", "JavaScript", "PHP"})
        QVERIFY(dialog->setChecked(QLatin1String(group), true));
    dialog->accept();

    MimeAppsService service(MimeAppsService::userFiles(), Id);
    for (const char *type : {"text/html", "text/css", "text/javascript", "application/javascript",
                             "application/x-php", "text/x-php"}) {
        QVERIFY2(service.isDefault(QLatin1String(type)), type);
    }
    QVERIFY(!service.isDefault(QStringLiteral("text/plain")));
    QVERIFY(!service.isDefault(QStringLiteral("text/x-python")));
    // Plik środowiska nie istniał — nie powstaje.
    QVERIFY(!QFile::exists(m_config.filePath(QStringLiteral("x-cinnamon-mimeapps.list"))));
    QVERIFY(fixture.prompts->errors.isEmpty());

    // Ponowne otwarcie pokazuje stan z pliku.
    dialog = openDialog(window);
    QCOMPARE(dialog->checkState(QStringLiteral("HTML")), Qt::Checked);
    QCOMPARE(dialog->checkState(QStringLiteral("PHP")), Qt::Checked);
    QCOMPARE(dialog->checkState(QStringLiteral("Python")), Qt::Unchecked);
    dialog->reject();
}

void TestDefaultApps::uncheckRestoresPreviousProgram()
{
    {
        QFile file(mimeAppsPath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Default Applications]\ntext/html=firefox.desktop;\n");
    }
    const QString original = readText(mimeAppsPath());
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    DefaultAppsDialog *dialog = openDialog(window);
    QCOMPARE(dialog->checkState(QStringLiteral("HTML")), Qt::Unchecked);
    dialog->setChecked(QStringLiteral("HTML"), true);
    dialog->accept();
    QCOMPARE(MimeApps::defaultsFor(readText(mimeAppsPath()), QStringLiteral("text/html")),
             (QStringList{Id, QStringLiteral("firefox.desktop")}));

    dialog = openDialog(window);
    dialog->setChecked(QStringLiteral("HTML"), false);
    dialog->accept();
    QCOMPARE(readText(mimeAppsPath()), original); // znów przeglądarka
}

void TestDefaultApps::cancelChangesNothing()
{
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    DefaultAppsDialog *dialog = openDialog(window);
    dialog->setChecked(QStringLiteral("CSS"), true);
    dialog->reject();
    QVERIFY(!QFile::exists(mimeAppsPath()));
}

void TestDefaultApps::partialGroupIsShownAndCompleted()
{
    {
        QFile file(mimeAppsPath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QStringLiteral("[Default Applications]\ntext/javascript=%1;\n").arg(Id).toUtf8());
    }
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    DefaultAppsDialog *dialog = openDialog(window);
    QCOMPARE(dialog->checkState(QStringLiteral("JavaScript")), Qt::PartiallyChecked);
    QVERIFY(dialog->typesToEnable().isEmpty() && dialog->typesToDisable().isEmpty());
    dialog->setChecked(QStringLiteral("JavaScript"), true);
    dialog->accept();
    MimeAppsService service(MimeAppsService::userFiles(), Id);
    QVERIFY(service.isDefault(QStringLiteral("text/javascript")));
    QVERIFY(service.isDefault(QStringLiteral("application/javascript")));
}

void TestDefaultApps::notInstalledAllowsOnlyRemoval()
{
    QVERIFY(QFile::remove(m_desktopEntry));
    {
        QFile file(mimeAppsPath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QStringLiteral("[Default Applications]\ntext/css=%1;\n").arg(Id).toUtf8());
    }
    WindowFixture fixture;
    MainWindow &window = *fixture.window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    DefaultAppsDialog *dialog = openDialog(window);
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("notInstalledLabel"))->isVisibleTo(dialog));
    QPushButton *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);

    dialog->setChecked(QStringLiteral("HTML"), true);
    QVERIFY(!ok->isEnabled()); // bez wpisu menu nie można ustawić programu
    dialog->setChecked(QStringLiteral("HTML"), false);
    dialog->setChecked(QStringLiteral("CSS"), false);
    QVERIFY(ok->isEnabled()); // zdjęcie starego ustawienia jest możliwe
    ok->click();
    QCOMPARE(readText(mimeAppsPath()), QStringLiteral("[Default Applications]\n"));
}

QTEST_MAIN(TestDefaultApps)
#include "tst_default_apps.moc"
