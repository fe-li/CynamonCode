// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Stan pliku na dysku i obserwacja zmian (D026, D027, D030; TESTY.md T18, T26):
// wykrycie zmiany, sam touch bez alarmu, usunięcie, konflikt zapisu,
// przeładowanie oraz zdarzenia FileWatchService także po podmianie przez rename.

#include "controllers/DocumentController.h"
#include "services/FileWatchService.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>

using namespace cynamoncode;

namespace {

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

bool writeBytes(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

} // namespace

class TestDiskChanges : public QObject
{
    Q_OBJECT

private slots:
    void freshlyOpenedIsUnchanged();
    void externalChangeBlocksPlainSave();
    void touchWithSameContentIsUnchanged();
    void deletedFileIsMissingAndCanBeSavedAgain();
    void reloadReadsNewContent();
    void failedReloadKeepsState();
    void watcherReportsWriteAndRenameReplace();
    void watcherIgnoresUnwatchedPath();
    void sameSizeAndMtimeChangeIsCaughtOnSave();
    void busyFileDoesNotStarveOthers();
    void reappearingFileIsWatchedAgain();
};

void TestDiskChanges::sameSizeAndMtimeChangeIsCaughtOnSave()
{
    // Regresja W1: zmiana tej samej długości z identycznym czasem modyfikacji.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "aaaa"));
    const QDateTime original = QFileInfo(path).lastModified();
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);

    QVERIFY(writeBytes(path, "bbbb"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(original, QFileDevice::FileModificationTime));
    file.close();

    QVERIFY(controller.checkDisk(opened.id, false) == DiskStatus::Changed);
    QVERIFY(controller.save(opened.id, QStringLiteral("cccc")).failure == SaveFailure::DiskChanged);
    QCOMPARE(readBytes(path), QByteArray("bbbb"));
}

void TestDiskChanges::busyFileDoesNotStarveOthers()
{
    // Regresja W4: plik zmieniany co 50 ms nie blokuje zgłoszeń innych plików.
    QTemporaryDir dir;
    const QString busy = dir.filePath(QStringLiteral("log.txt"));
    const QString quiet = dir.filePath(QStringLiteral("cichy.txt"));
    QVERIFY(writeBytes(busy, "0"));
    QVERIFY(writeBytes(quiet, "0"));

    FileWatchService watcher;
    watcher.watch(busy);
    watcher.watch(quiet);
    QSignalSpy spy(&watcher, &FileWatchService::fileChanged);

    QElapsedTimer clock;
    clock.start();
    QVERIFY(writeBytes(quiet, "zmiana"));
    bool quietReported = false;
    for (int i = 0; i < 30 && !quietReported; ++i) {
        QVERIFY(writeBytes(busy, QByteArray::number(i)));
        QTest::qWait(50);
        for (const QList<QVariant> &call : std::as_const(spy))
            quietReported = quietReported || call.first().toString() == quiet;
    }
    QVERIFY(quietReported);
    QVERIFY2(clock.elapsed() < 1000, qPrintable(QString::number(clock.elapsed())));
}

void TestDiskChanges::reappearingFileIsWatchedAgain()
{
    // Regresja W3: plik usunięty i odtworzony po dłuższej przerwie.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "1"));
    FileWatchService watcher;
    watcher.watch(path);
    QSignalSpy spy(&watcher, &FileWatchService::fileChanged);

    QVERIFY(QFile::remove(path));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
    QTest::qWait(300);
    const int before = spy.count();
    QVERIFY(writeBytes(path, "wrócił"));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > before, 5000);

    // I dalsze zmiany są znowu zgłaszane.
    QTest::qWait(FileWatchService::DebounceMs * 3);
    const int afterReturn = spy.count();
    QVERIFY(writeBytes(path, "kolejna zmiana"));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > afterReturn, 5000);
}

void TestDiskChanges::freshlyOpenedIsUnchanged()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "treść"));
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Unchanged);
    QVERIFY(controller.checkDisk(controller.createNew()) == DiskStatus::Unchanged);
}

void TestDiskChanges::externalChangeBlocksPlainSave()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "moja"));
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);

    QVERIFY(writeBytes(path, "zmiana z innego programu"));
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Changed);

    const SaveOutcome refused = controller.save(opened.id, QStringLiteral("moja wersja"));
    QVERIFY(refused.failure == SaveFailure::DiskChanged);
    QCOMPARE(readBytes(path), QByteArray("zmiana z innego programu"));

    QVERIFY(controller.save(opened.id, QStringLiteral("moja wersja"), true).ok());
    QCOMPARE(readBytes(path), QByteArray("moja wersja"));
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Unchanged);
    QVERIFY(controller.save(opened.id, QStringLiteral("kolejny zapis")).ok());
}

void TestDiskChanges::touchWithSameContentIsUnchanged()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "bez zmian"));
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(30),
                             QFileDevice::FileModificationTime));
    file.close();
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Unchanged);
    QVERIFY(controller.save(opened.id, QStringLiteral("zapis po touch")).ok());
}

void TestDiskChanges::deletedFileIsMissingAndCanBeSavedAgain()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "treść"));
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);

    QVERIFY(QFile::remove(path));
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Missing);
    QVERIFY(controller.document(opened.id)->missingOnDisk);

    QVERIFY(controller.save(opened.id, QStringLiteral("odtworzony")).ok());
    QCOMPARE(readBytes(path), QByteArray("odtworzony"));
    QVERIFY(!controller.document(opened.id)->missingOnDisk);
}

void TestDiskChanges::reloadReadsNewContent()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "stara\n"));
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);

    QVERIFY(writeBytes(path, "nowa treść\r\ndruga\r\n"));
    const OpenOutcome reloaded = controller.reload(opened.id);
    QVERIFY(reloaded.status == OpenStatus::Opened);
    QCOMPARE(reloaded.text, QStringLiteral("nowa treść\r\ndruga\r\n"));
    QVERIFY(controller.document(opened.id)->lineEnding == LineEnding::CRLF);
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Unchanged);
}

void TestDiskChanges::failedReloadKeepsState()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "dobra\n"));
    DocumentController controller;
    const OpenOutcome opened = controller.open(path);

    QVERIFY(writeBytes(path, QByteArray("zepsute \xFF bajty")));
    const OpenOutcome reloaded = controller.reload(opened.id);
    QVERIFY(reloaded.status == OpenStatus::Failed);
    QVERIFY(reloaded.failure == OpenFailure::InvalidUtf8);
    // Stan sprzed próby: plik nadal różni się od tego, co jest w edytorze.
    QVERIFY(controller.checkDisk(opened.id) == DiskStatus::Changed);
}

void TestDiskChanges::watcherReportsWriteAndRenameReplace()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("obserwowany.txt"));
    QVERIFY(writeBytes(path, "1"));

    FileWatchService watcher;
    watcher.watch(path);
    QSignalSpy spy(&watcher, &FileWatchService::fileChanged);

    QVERIFY(writeBytes(path, "22"));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
    QCOMPARE(spy.first().first().toString(), path);

    // Podmiana przez rename (jak QSaveFile, git, vim) — obserwacja ma przetrwać.
    const QString temporary = dir.filePath(QStringLiteral("tmp.txt"));
    QVERIFY(writeBytes(temporary, "333"));
    const int before = spy.count();
    QCOMPARE(std::rename(QFile::encodeName(temporary).constData(),
                         QFile::encodeName(path).constData()),
             0);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > before, 5000);

    const int afterRename = spy.count();
    QTest::qWait(FileWatchService::DebounceMs * 3);
    QVERIFY(writeBytes(path, "4444"));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > afterRename, 5000);
}

void TestDiskChanges::watcherIgnoresUnwatchedPath()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "1"));
    FileWatchService watcher;
    watcher.watch(path);
    watcher.unwatch(path);
    QSignalSpy spy(&watcher, &FileWatchService::fileChanged);
    QVERIFY(writeBytes(path, "22"));
    QTest::qWait(FileWatchService::DebounceMs * 4);
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(TestDiskChanges)
#include "tst_disk_changes.moc"
