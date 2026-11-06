// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Domyślny program dla typów plików (D047): edycja mimeapps.list bez naruszania
// pozostałych wpisów, przywracanie poprzedniego programu, pierwszeństwo plików
// środowiska, zapis atomowy i błędy zapisu.

#include "services/MimeAppsService.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

using namespace cynamoncode;

namespace {

const QString Id = QStringLiteral("pl.feliszewski.CynamonCode.desktop");

QString readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

bool writeText(const QString &path, const QString &text)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(text.toUtf8()) >= 0;
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void restoreEnv(const char *name, const QByteArray &value, bool wasSet)
{
    if (wasSet)
        qputenv(name, value);
    else
        qunsetenv(name);
}

} // namespace

class TestMimeApps : public QObject
{
    Q_OBJECT

private slots:
    void parsesOnlyDefaultApplicationsGroup();
    void enableCreatesGroupInEmptyFile();
    void enableThenDisableRestoresPreviousDefault();
    void keepsOtherLinesAndGroups();
    void disableRemovesOwnOnlyEntry();
    void enableIsIdempotent();
    void lastDuplicateKeyWinsLikeGLib();
    void repeatedDefaultGroupsActAsOne();
    void crlfFileKeepsLineEndings();
    void refusesInvalidUtf8AndKeepsBom();
    void keepsSymlinkAndPermissions();
    void disableWithoutEntryChangesNothing();
    void userFilesFollowXdgVariables();
    void serviceAppliesToPrimaryAndExistingDesktopFile();
    void desktopSpecificFileHasPriority();
    void applyReportsWriteError();
};

void TestMimeApps::parsesOnlyDefaultApplicationsGroup()
{
    const QString content = QStringLiteral("[Added Associations]\n"
                                           "text/html=inny.desktop;\n"
                                           "# komentarz\n"
                                           "[Default Applications]\n"
                                           "text/html = firefox.desktop;chromium.desktop\n"
                                           "text/css=\n");
    QCOMPARE(MimeApps::defaultsFor(content, QStringLiteral("text/html")),
             (QStringList{QStringLiteral("firefox.desktop"), QStringLiteral("chromium.desktop")}));
    QVERIFY(MimeApps::defaultsFor(content, QStringLiteral("text/css")).isEmpty());
    QVERIFY(MimeApps::defaultsFor(content, QStringLiteral("text/plain")).isEmpty());
}

void TestMimeApps::enableCreatesGroupInEmptyFile()
{
    QCOMPARE(MimeApps::withDefault(QString(), QStringLiteral("text/html"), Id, true),
             QStringLiteral("[Default Applications]\ntext/html=%1;\n").arg(Id));
    // Plik bez grupy: nowa grupa na końcu, oddzielona pustym wierszem.
    QCOMPARE(MimeApps::withDefault(QStringLiteral("[Added Associations]\ntext/css=a.desktop;"),
                                   QStringLiteral("text/css"), Id, true),
             QStringLiteral("[Added Associations]\ntext/css=a.desktop;\n\n"
                            "[Default Applications]\ntext/css=%1;\n")
                 .arg(Id));
}

void TestMimeApps::enableThenDisableRestoresPreviousDefault()
{
    const QString original =
        QStringLiteral("[Default Applications]\ntext/html=firefox.desktop;\n");
    const QString enabled = MimeApps::withDefault(original, QStringLiteral("text/html"), Id, true);
    QCOMPARE(MimeApps::defaultsFor(enabled, QStringLiteral("text/html")),
             (QStringList{Id, QStringLiteral("firefox.desktop")}));
    QCOMPARE(MimeApps::withDefault(enabled, QStringLiteral("text/html"), Id, false), original);
}

void TestMimeApps::keepsOtherLinesAndGroups()
{
    const QString original = QStringLiteral("# ustawienia użytkownika\n"
                                            "[Default Applications]\n"
                                            "image/png=eog.desktop;\n"
                                            "text/css=xed.desktop;\n"
                                            "\n"
                                            "[Added Associations]\n"
                                            "text/css=xed.desktop;\n");
    const QString updated = MimeApps::withDefault(original, QStringLiteral("text/css"), Id, true);
    QCOMPARE(updated, QStringLiteral("# ustawienia użytkownika\n"
                                     "[Default Applications]\n"
                                     "image/png=eog.desktop;\n"
                                     "text/css=%1;xed.desktop;\n"
                                     "\n"
                                     "[Added Associations]\n"
                                     "text/css=xed.desktop;\n")
                          .arg(Id));
    // Nowy typ trafia do istniejącej grupy, nie do [Added Associations].
    const QString withHtml = MimeApps::withDefault(updated, QStringLiteral("text/html"), Id, true);
    QVERIFY(withHtml.startsWith(
        QStringLiteral("# ustawienia użytkownika\n[Default Applications]\ntext/html=%1;\n").arg(Id)));
    QVERIFY(withHtml.endsWith(QStringLiteral("[Added Associations]\ntext/css=xed.desktop;\n")));
}

void TestMimeApps::disableRemovesOwnOnlyEntry()
{
    const QString content =
        QStringLiteral("[Default Applications]\ntext/html=%1;\nimage/png=eog.desktop;\n").arg(Id);
    QCOMPARE(MimeApps::withDefault(content, QStringLiteral("text/html"), Id, false),
             QStringLiteral("[Default Applications]\nimage/png=eog.desktop;\n"));
}

void TestMimeApps::enableIsIdempotent()
{
    const QString once = MimeApps::withDefault(QString(), QStringLiteral("text/css"), Id, true);
    QCOMPARE(MimeApps::withDefault(once, QStringLiteral("text/css"), Id, true), once);
}

void TestMimeApps::lastDuplicateKeyWinsLikeGLib()
{
    // GKeyFile (Cinnamon, Nemo): z powtórzonych kluczy obowiązuje ostatni.
    const QString original = QStringLiteral("[Default Applications]\n"
                                            "text/css=a.desktop;\n"
                                            "text/css=b.desktop;\n");
    QCOMPARE(MimeApps::defaultsFor(original, QStringLiteral("text/css")),
             QStringList{QStringLiteral("b.desktop")});
    const QString enabled = MimeApps::withDefault(original, QStringLiteral("text/css"), Id, true);
    QCOMPARE(enabled, QStringLiteral("[Default Applications]\n"
                                     "text/css=a.desktop;\n"
                                     "text/css=%1;b.desktop;\n")
                          .arg(Id));
    // Odznaczenie przywraca plik bajt w bajt — i program „b”.
    QCOMPARE(MimeApps::withDefault(enabled, QStringLiteral("text/css"), Id, false), original);
}

void TestMimeApps::repeatedDefaultGroupsActAsOne()
{
    const QString original = QStringLiteral("[Default Applications]\n"
                                            "image/png=eog.desktop;\n"
                                            "[Added Associations]\n"
                                            "text/html=x.desktop;\n"
                                            "[Default Applications]\n"
                                            "text/html=firefox.desktop;\n");
    QCOMPARE(MimeApps::defaultsFor(original, QStringLiteral("image/png")),
             QStringList{QStringLiteral("eog.desktop")});
    const QString html = MimeApps::withDefault(original, QStringLiteral("text/html"), Id, true);
    QCOMPARE(html, QStringLiteral("[Default Applications]\n"
                                  "image/png=eog.desktop;\n"
                                  "[Added Associations]\n"
                                  "text/html=x.desktop;\n"
                                  "[Default Applications]\n"
                                  "text/html=%1;firefox.desktop;\n")
                       .arg(Id));
    // Nowy klucz trafia do ostatniej grupy [Default Applications].
    const QString css = MimeApps::withDefault(original, QStringLiteral("text/css"), Id, true);
    QVERIFY(css.endsWith(QStringLiteral("[Default Applications]\ntext/css=%1;\n"
                                        "text/html=firefox.desktop;\n")
                             .arg(Id)));
    QCOMPARE(MimeApps::defaultsFor(css, QStringLiteral("text/css")), QStringList{Id});
}

void TestMimeApps::crlfFileKeepsLineEndings()
{
    const QString original =
        QStringLiteral("[Default Applications]\r\ntext/html=firefox.desktop;\r\n");
    const QString enabled = MimeApps::withDefault(original, QStringLiteral("text/html"), Id, true);
    QCOMPARE(enabled,
             QStringLiteral("[Default Applications]\r\ntext/html=%1;firefox.desktop;\r\n").arg(Id));
    QCOMPARE(MimeApps::withDefault(enabled, QStringLiteral("text/html"), Id, false), original);
    const QString css = MimeApps::withDefault(original, QStringLiteral("text/css"), Id, true);
    QCOMPARE(css.count(QStringLiteral("\r\n")), css.count(QLatin1Char('\n')));
}

void TestMimeApps::refusesInvalidUtf8AndKeepsBom()
{
    QTemporaryDir dir;
    // Bajt 0xE9 (Latin-1) w komentarzu — plik zostaje nietknięty, bez U+FFFD.
    const QString latin = dir.filePath(QStringLiteral("latin1.list"));
    const QByteArray latinBytes("# caf\xE9\n[Default Applications]\ntext/html=firefox.desktop;\n");
    QVERIFY(writeBytes(latin, latinBytes));
    MimeAppsService latinService({latin}, Id);
    QString error;
    QVERIFY(!latinService.apply({QStringLiteral("text/html")}, {}, &error));
    QVERIFY2(error.contains(QStringLiteral("UTF-8")), qPrintable(error));
    QCOMPARE(readBytes(latin), latinBytes);

    // BOM jest zachowany i nie psuje rozpoznania grupy.
    const QString bom = dir.filePath(QStringLiteral("bom.list"));
    const QByteArray bomBytes("\xEF\xBB\xBF[Default Applications]\ntext/html=firefox.desktop;\n");
    QVERIFY(writeBytes(bom, bomBytes));
    MimeAppsService bomService({bom}, Id);
    QVERIFY(bomService.apply({QStringLiteral("text/html")}, {}, &error));
    QCOMPARE(readBytes(bom),
             QByteArray("\xEF\xBB\xBF[Default Applications]\ntext/html=") + Id.toUtf8()
                 + QByteArray(";firefox.desktop;\n"));
    QVERIFY(bomService.apply({}, {QStringLiteral("text/html")}, &error));
    QCOMPARE(readBytes(bom), bomBytes);
}

void TestMimeApps::keepsSymlinkAndPermissions()
{
    QTemporaryDir dir;
    const QString target = dir.filePath(QStringLiteral("dotfiles/mimeapps.list"));
    const QString link = dir.filePath(QStringLiteral("mimeapps.list"));
    QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("dotfiles"))));
    QVERIFY(writeText(target, QStringLiteral("[Default Applications]\n")));
    QVERIFY(QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner)); // 0600
    QVERIFY(QFile::link(target, link));

    MimeAppsService service({link}, Id);
    QVERIFY(service.apply({QStringLiteral("text/css")}, {}));
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(MimeApps::defaultsFor(readText(target), QStringLiteral("text/css")), QStringList{Id});
    QCOMPARE(QFile::permissions(target) & 0x7777,
             QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser
                                | QFile::WriteUser));
}

void TestMimeApps::disableWithoutEntryChangesNothing()
{
    const QString content = QStringLiteral("[Default Applications]\ntext/html=firefox.desktop\n");
    QCOMPARE(MimeApps::withDefault(content, QStringLiteral("text/html"), Id, false), content);
    QCOMPARE(MimeApps::withDefault(QString(), QStringLiteral("text/html"), Id, false), QString());
}

void TestMimeApps::userFilesFollowXdgVariables()
{
    QTemporaryDir dir;
    const QByteArray oldConfig = qgetenv("XDG_CONFIG_HOME");
    const QByteArray oldDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    const bool hadConfig = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
    const bool hadDesktop = qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP");
    qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
    qputenv("XDG_CURRENT_DESKTOP", "X-Cinnamon:GNOME");
    const QStringList files = MimeAppsService::userFiles();
    restoreEnv("XDG_CONFIG_HOME", oldConfig, hadConfig);
    restoreEnv("XDG_CURRENT_DESKTOP", oldDesktop, hadDesktop);

    QCOMPARE(files, (QStringList{dir.filePath(QStringLiteral("x-cinnamon-mimeapps.list")),
                                 dir.filePath(QStringLiteral("gnome-mimeapps.list")),
                                 dir.filePath(QStringLiteral("mimeapps.list"))}));
}

void TestMimeApps::serviceAppliesToPrimaryAndExistingDesktopFile()
{
    QTemporaryDir dir;
    const QString desktopFile = dir.filePath(QStringLiteral("x-cinnamon-mimeapps.list"));
    const QString otherDesktopFile = dir.filePath(QStringLiteral("gnome-mimeapps.list"));
    const QString primary = dir.filePath(QStringLiteral("podkatalog/mimeapps.list"));
    QVERIFY(writeText(desktopFile, QStringLiteral("[Default Applications]\n")));
    MimeAppsService service({desktopFile, otherDesktopFile, primary}, Id);

    QVERIFY(!service.isDefault(QStringLiteral("text/html")));
    QString error;
    QVERIFY2(service.apply({QStringLiteral("text/html"), QStringLiteral("text/css")}, {}, &error),
             qPrintable(error));
    QVERIFY(service.isDefault(QStringLiteral("text/html")));
    QVERIFY(service.isDefault(QStringLiteral("text/css")));
    QVERIFY(!service.isDefault(QStringLiteral("text/plain")));
    // Plik główny utworzony (z katalogiem), istniejący plik środowiska uzupełniony,
    // nieistniejący nie powstaje.
    QCOMPARE(MimeApps::defaultsFor(readText(primary), QStringLiteral("text/css")), QStringList{Id});
    QCOMPARE(MimeApps::defaultsFor(readText(desktopFile), QStringLiteral("text/html")),
             QStringList{Id});
    QVERIFY(!QFile::exists(otherDesktopFile));

    QVERIFY(service.apply({}, {QStringLiteral("text/html"), QStringLiteral("text/css")}, &error));
    QVERIFY(!service.isDefault(QStringLiteral("text/html")));
    QCOMPARE(readText(desktopFile), QStringLiteral("[Default Applications]\n"));
    QCOMPARE(readText(primary), QStringLiteral("[Default Applications]\n"));
}

void TestMimeApps::desktopSpecificFileHasPriority()
{
    QTemporaryDir dir;
    const QString desktopFile = dir.filePath(QStringLiteral("x-cinnamon-mimeapps.list"));
    const QString primary = dir.filePath(QStringLiteral("mimeapps.list"));
    QVERIFY(writeText(desktopFile,
                      QStringLiteral("[Default Applications]\ntext/html=firefox.desktop;\n")));
    QVERIFY(writeText(primary, QStringLiteral("[Default Applications]\ntext/html=%1;\n").arg(Id)));
    MimeAppsService service({desktopFile, primary}, Id);
    // Plik środowiska wygrywa — CynamonCode nie jest domyślny, choć jest w mimeapps.list.
    QVERIFY(!service.isDefault(QStringLiteral("text/html")));
    QVERIFY(service.apply({QStringLiteral("text/html")}, {}));
    QVERIFY(service.isDefault(QStringLiteral("text/html")));
    QCOMPARE(MimeApps::defaultsFor(readText(desktopFile), QStringLiteral("text/html")),
             (QStringList{Id, QStringLiteral("firefox.desktop")}));
}

void TestMimeApps::applyReportsWriteError()
{
    if (::geteuid() == 0)
        QSKIP("root pomija uprawnienia katalogu");
    QTemporaryDir dir;
    const QString locked = dir.filePath(QStringLiteral("zablokowany"));
    QVERIFY(QDir().mkpath(locked));
    QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
    MimeAppsService service({QDir(locked).filePath(QStringLiteral("mimeapps.list"))}, Id);
    QString error;
    QVERIFY(!service.apply({QStringLiteral("text/html")}, {}, &error));
    QVERIFY(error.contains(QStringLiteral("mimeapps.list")));
    QVERIFY(!service.isDefault(QStringLiteral("text/html")));
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

QTEST_GUILESS_MAIN(TestMimeApps)
#include "tst_mime_apps.moc"
