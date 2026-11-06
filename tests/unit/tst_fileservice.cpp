// FileService: odczyt surowych bajtów i atomowy zapis QSaveFile; błędy IO
// nie mogą uszkodzić istniejącego pliku (TESTY.md T05, T16; D017).

#include "services/FileService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

using namespace cynamoncode;

namespace {

bool isRoot()
{
    return ::geteuid() == 0;
}

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

const QFileDevice::Permissions AllPermissionBits =
    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
    | QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
    | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;

} // namespace

class TestFileService : public QObject
{
    Q_OBJECT

private slots:
    void writeThenReadKeepsBytes();
    void writesEmptyFile();
    void readMissingFile();
    void readDirectory();
    void refusesReadOnlyFile();
    void refusesNonWritableDirectory();
    void keepsPermissions();
    void writesThroughSymlink();
    void refusesBrokenSymlink();
};

void TestFileService::writeThenReadKeepsBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("plik.txt"));
    const QByteArray data("Zażółć gęślą jaźń 🐈\r\nlinia\n");

    const FileWriteResult written = FileService::writeAtomically(path, data);
    QVERIFY2(written.ok(), qPrintable(written.systemMessage));

    const FileReadResult read = FileService::readAll(path);
    QVERIFY(read.ok());
    QCOMPARE(read.data, data);
}

void TestFileService::writesEmptyFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("pusty.txt"));
    QVERIFY(writeBytes(path, "stara treść"));

    QVERIFY(FileService::writeAtomically(path, QByteArray()).ok());
    QCOMPARE(QFileInfo(path).size(), qint64(0));
}

void TestFileService::readMissingFile()
{
    QTemporaryDir dir;
    const FileReadResult read = FileService::readAll(dir.filePath(QStringLiteral("brak.txt")));
    QVERIFY(read.error == FileError::NotFound);
}

void TestFileService::readDirectory()
{
    QTemporaryDir dir;
    const FileReadResult read = FileService::readAll(dir.path());
    QVERIFY(read.error == FileError::NotRegularFile);
}

void TestFileService::refusesReadOnlyFile()
{
    if (isRoot())
        QSKIP("Użytkownik root omija prawa dostępu.");

    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("tylko-odczyt.txt"));
    QVERIFY(writeBytes(path, "oryginał"));
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser));

    const FileWriteResult written = FileService::writeAtomically(path, "nowa treść");
    QVERIFY(written.error == FileError::NotWritable);
    QCOMPARE(readBytes(path), QByteArray("oryginał"));
}

void TestFileService::refusesNonWritableDirectory()
{
    if (isRoot())
        QSKIP("Użytkownik root omija prawa dostępu.");

    QTemporaryDir dir;
    const QString subdir = dir.filePath(QStringLiteral("zablokowany"));
    QVERIFY(QDir().mkdir(subdir));
    const QString path = subdir + QStringLiteral("/plik.txt");
    QVERIFY(writeBytes(path, "oryginał"));
    QVERIFY(QFile::setPermissions(subdir, QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    const FileWriteResult written = FileService::writeAtomically(path, "nowa treść");

    QFile::setPermissions(subdir, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner); // sprzątanie QTemporaryDir
    QVERIFY(!written.ok());
    QCOMPARE(readBytes(path), QByteArray("oryginał"));
}

void TestFileService::keepsPermissions()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("uprawnienia.txt"));
    QVERIFY(writeBytes(path, "a"));
    const QFileDevice::Permissions expected =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup;
    QVERIFY(QFile::setPermissions(path, expected));

    QVERIFY(FileService::writeAtomically(path, "b").ok());
    QCOMPARE((QFile::permissions(path) & AllPermissionBits).toInt(), expected.toInt());
}

void TestFileService::writesThroughSymlink()
{
    QTemporaryDir dir;
    const QString target = dir.filePath(QStringLiteral("cel.txt"));
    const QString link = dir.filePath(QStringLiteral("dowiazanie.txt"));
    QVERIFY(writeBytes(target, "stara"));
    QVERIFY(QFile::link(target, link));

    QVERIFY(FileService::writeAtomically(link, "nowa").ok());
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(readBytes(target), QByteArray("nowa"));
}

void TestFileService::refusesBrokenSymlink()
{
    QTemporaryDir dir;
    const QString link = dir.filePath(QStringLiteral("zerwane.txt"));
    QVERIFY(QFile::link(dir.filePath(QStringLiteral("nie-istnieje.txt")), link));

    QVERIFY(FileService::writeAtomically(link, "x").error == FileError::BrokenSymlink);
    QVERIFY(FileService::readAll(link).error == FileError::BrokenSymlink);
}

QTEST_GUILESS_MAIN(TestFileService)
#include "tst_fileservice.moc"
