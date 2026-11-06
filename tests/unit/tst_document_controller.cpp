// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// DocumentController bez GUI: deduplikacja kart, roundtrip zapisu bajt w bajt,
// odmowa plików nie-UTF-8 (TESTY.md T03, T06, T07, T12–T15).

#include "controllers/DocumentController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

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

class TestDocumentController : public QObject
{
    Q_OBJECT

private slots:
    void openingSameFileTwiceReturnsExistingDocument();
    void sameNameInDifferentDirsGivesTwoDocuments();
    void saveRoundtripKeepsBytes_data();
    void saveRoundtripKeepsBytes();
    void saveAsAssignsPath();
    void saveAsRefusesFileOpenInAnotherDocument();
    void saveWithoutPathIsRefused();
    void refusesInvalidUtf8AndBinary_data();
    void refusesInvalidUtf8AndBinary();
    void closedDocumentCanBeOpenedAgain();
    void symlinkAndTargetAreOneDocument();
    void languageIsDetectedOnOpenAndSaveAs();
    void manualLanguageIsKept();
};

void TestDocumentController::openingSameFileTwiceReturnsExistingDocument()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "treść"));

    DocumentController controller;
    const OpenOutcome first = controller.open(path);
    QVERIFY(first.status == OpenStatus::Opened);
    QCOMPARE(first.text, QStringLiteral("treść"));

    const OpenOutcome second = controller.open(path);
    QVERIFY(second.status == OpenStatus::AlreadyOpen);
    QCOMPARE(second.id, first.id);

    const OpenOutcome viaDot = controller.open(dir.path() + QStringLiteral("/./a.txt"));
    QVERIFY(viaDot.status == OpenStatus::AlreadyOpen);
    QCOMPARE(viaDot.id, first.id);
    QCOMPARE(controller.documentCount(), qsizetype(1));
}

void TestDocumentController::sameNameInDifferentDirsGivesTwoDocuments()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("x")));
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("y")));
    const QString first = dir.filePath(QStringLiteral("x/plik.txt"));
    const QString second = dir.filePath(QStringLiteral("y/plik.txt"));
    QVERIFY(writeBytes(first, "x"));
    QVERIFY(writeBytes(second, "y"));

    DocumentController controller;
    const OpenOutcome a = controller.open(first);
    const OpenOutcome b = controller.open(second);
    QVERIFY(a.status == OpenStatus::Opened);
    QVERIFY(b.status == OpenStatus::Opened);
    QVERIFY(a.id != b.id);
    QCOMPARE(b.text, QStringLiteral("y"));
}

void TestDocumentController::saveRoundtripKeepsBytes_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("polskie i emoji, LF") << QByteArray("Zażółć gęślą jaźń 😀\nkoniec\n");
    QTest::newRow("CRLF") << QByteArray("linia 1\r\nlinia 2\r\n");
    QTest::newRow("mieszane") << QByteArray("a\nb\r\nc");
    QTest::newRow("BOM") << (QByteArray("\xEF\xBB\xBF", 3) + QByteArray("z BOM-em\n"));
    QTest::newRow("pusty") << QByteArray();
}

void TestDocumentController::saveRoundtripKeepsBytes()
{
    QFETCH(QByteArray, bytes);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("roundtrip.txt"));
    QVERIFY(writeBytes(path, bytes));

    DocumentController controller;
    const OpenOutcome opened = controller.open(path);
    QVERIFY(opened.status == OpenStatus::Opened);

    const SaveOutcome saved = controller.save(opened.id, opened.text);
    QVERIFY2(saved.ok(), qPrintable(saved.systemMessage));
    QCOMPARE(readBytes(path), bytes);
}

void TestDocumentController::saveAsAssignsPath()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("nowy.txt"));

    DocumentController controller;
    const DocumentId id = controller.createNew();
    QVERIFY(controller.document(id)->isUntitled());

    QVERIFY(controller.saveAs(id, path, QStringLiteral("tekst ąę")).ok());
    const DocumentState *document = controller.document(id);
    QVERIFY(!document->isUntitled());
    QCOMPARE(document->filePath, QFileInfo(path).absoluteFilePath());
    QCOMPARE(readBytes(path), QStringLiteral("tekst ąę").toUtf8());

    // Po zapisaniu ten sam plik nie otworzy się w drugiej karcie.
    QVERIFY(controller.open(path).status == OpenStatus::AlreadyOpen);
}

void TestDocumentController::saveAsRefusesFileOpenInAnotherDocument()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("zajety.txt"));
    QVERIFY(writeBytes(path, "oryginał"));

    DocumentController controller;
    QVERIFY(controller.open(path).status == OpenStatus::Opened);
    const DocumentId other = controller.createNew();

    const SaveOutcome saved = controller.saveAs(other, path, QStringLiteral("nadpisanie"));
    QVERIFY(saved.failure == SaveFailure::OpenInAnotherDocument);
    QCOMPARE(readBytes(path), QByteArray("oryginał"));
    QVERIFY(controller.document(other)->isUntitled());
}

void TestDocumentController::saveWithoutPathIsRefused()
{
    DocumentController controller;
    const DocumentId id = controller.createNew();
    QVERIFY(controller.save(id, QStringLiteral("x")).failure == SaveFailure::NoPath);
}

void TestDocumentController::refusesInvalidUtf8AndBinary_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<int>("failure");
    QTest::newRow("Latin-1") << QByteArray("za\xBF\xF3\xB3" "\xE6") << int(OpenFailure::InvalidUtf8);
    QTest::newRow("NUL") << QByteArray("ab\0cd", 5) << int(OpenFailure::Binary);
}

void TestDocumentController::refusesInvalidUtf8AndBinary()
{
    QFETCH(QByteArray, bytes);
    QFETCH(int, failure);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("zly.bin"));
    QVERIFY(writeBytes(path, bytes));

    DocumentController controller;
    const OpenOutcome outcome = controller.open(path);
    QVERIFY(outcome.status == OpenStatus::Failed);
    QCOMPARE(int(outcome.failure), failure);
    QCOMPARE(controller.documentCount(), qsizetype(0));
    QCOMPARE(readBytes(path), bytes);
}

void TestDocumentController::closedDocumentCanBeOpenedAgain()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeBytes(path, "a"));

    DocumentController controller;
    const OpenOutcome first = controller.open(path);
    controller.close(first.id);
    QCOMPARE(controller.documentCount(), qsizetype(0));

    const OpenOutcome again = controller.open(path);
    QVERIFY(again.status == OpenStatus::Opened);
    QVERIFY(again.id != first.id);
}

void TestDocumentController::symlinkAndTargetAreOneDocument()
{
    QTemporaryDir dir;
    const QString target = dir.filePath(QStringLiteral("cel.txt"));
    const QString link = dir.filePath(QStringLiteral("dowiazanie.txt"));
    QVERIFY(writeBytes(target, "treść"));
    QVERIFY(QFile::link(target, link));

    DocumentController controller;
    const OpenOutcome viaLink = controller.open(link);
    QVERIFY(viaLink.status == OpenStatus::Opened);
    // Ścieżka użytkownika zostaje (dowiązanie), tożsamość — po pliku docelowym.
    QCOMPARE(controller.document(viaLink.id)->filePath, QFileInfo(link).absoluteFilePath());

    const OpenOutcome viaTarget = controller.open(target);
    QVERIFY(viaTarget.status == OpenStatus::AlreadyOpen);
    QCOMPARE(viaTarget.id, viaLink.id);
}

void TestDocumentController::languageIsDetectedOnOpenAndSaveAs()
{
    QTemporaryDir dir;
    const QString script = dir.filePath(QStringLiteral("narzedzie"));
    QVERIFY(writeBytes(script, "#!/usr/bin/env python3\r\nprint(1)\r\n"));

    DocumentController controller;
    const OpenOutcome opened = controller.open(script);
    QCOMPARE(int(controller.document(opened.id)->language), int(LanguageId::Python));

    const DocumentId id = controller.createNew();
    QCOMPARE(int(controller.document(id)->language), int(LanguageId::Plain));
    QVERIFY(controller.saveAs(id, dir.filePath(QStringLiteral("styl.css")), QStringLiteral("a{}"))
                .ok());
    QCOMPARE(int(controller.document(id)->language), int(LanguageId::Css));
}

void TestDocumentController::manualLanguageIsKept()
{
    QTemporaryDir dir;
    DocumentController controller;
    const DocumentId id = controller.createNew();
    controller.setLanguage(id, LanguageId::Sql);
    QVERIFY(controller.saveAs(id, dir.filePath(QStringLiteral("x.py")), QStringLiteral("select 1"))
                .ok());
    QCOMPARE(int(controller.document(id)->language), int(LanguageId::Sql));
}

QTEST_GUILESS_MAIN(TestDocumentController)
#include "tst_document_controller.moc"
