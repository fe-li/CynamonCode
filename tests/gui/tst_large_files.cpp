// Duże pliki (D039): pytanie przed otwarciem pliku > 20 MiB oraz testy
// wydajności dla pliku kilku MiB — z łagodnymi limitami czasu, bo build Debug
// w maszynie wirtualnej jest wolniejszy. Czasy są wypisywane (qInfo).

#include "TestHelpers.h"
#include "controllers/DocumentController.h"
#include "ui/SearchResultsDock.h"

#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTreeWidget>

using namespace cynamoncode;
using namespace cynamoncode::testing;

namespace {

constexpr qint64 GenerousLimitMs = 15000;

QByteArray makeText(qsizetype lines)
{
    QByteArray text;
    text.reserve(lines * 56);
    for (qsizetype i = 0; i < lines; ++i)
        text += "linia " + QByteArray::number(i) + ": zażółć gęślą jaźń kot pies\n";
    return text;
}

} // namespace

class TestLargeFiles : public QObject
{
    Q_OBJECT

private slots:
    void veryLargeFileAsksFirst();
    void severalMegabytesStayResponsive();
    void longSingleLineSearchIsFast();
};

void TestLargeFiles::longSingleLineSearchIsFast()
{
    // Regresja przeglądu E4 (W3): zminifikowany plik w jednym wierszu.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("min.js"));
    QByteArray bytes;
    for (int i = 0; i < 6000; ++i)
        bytes += "var a" + QByteArray::number(i) + "=1;" + QByteArray(480, 'x') + "kot;";
    QVERIFY(writeBytes(path, bytes)); // ok. 3 MiB, jeden wiersz
    QVERIFY(!bytes.contains('\n'));

    WindowFixture f;
    QVERIFY(f.window->openFile(path));
    SearchOptions options;
    options.pattern = QStringLiteral("kot");
    QElapsedTimer timer;
    timer.start();
    QCOMPARE(f.window->findInAllDocuments(options), 6000);
    const qint64 ms = timer.elapsed();
    qInfo("jeden wiersz %.1f MiB, 6000 trafień: %lld ms", double(bytes.size()) / 1048576.0, ms);
    QVERIFY2(ms < GenerousLimitMs, qPrintable(QString::number(ms)));

    QTreeWidgetItem *first = f.window->searchResults()->tree()->topLevelItem(0)->child(0);
    QCOMPARE(first->text(0), QStringLiteral("1"));
    QVERIFY2(first->text(1).contains(QStringLiteral("kot")), qPrintable(first->text(1)));
    QVERIFY(first->text(1).size() <= 202); // fragment z wielokropkami
}

void TestLargeFiles::veryLargeFileAsksFirst()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("duzy.log"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray chunk(1024 * 1024, 'a');
        for (int i = 0; i < 21; ++i)
            QVERIFY(file.write(chunk) == chunk.size());
    }
    QVERIFY(QFileInfo(path).size() > DocumentController::LargeFileWarningBytes);

    WindowFixture f;
    f.prompts->openLargeAnswer = false;
    QVERIFY(!f.window->openFile(path));
    QCOMPARE(f.prompts->largeFileQuestions, 1);
    QCOMPARE(f.window->documentCount(), 1);
    QVERIFY(f.window->currentEditor()->text().isEmpty());
}

void TestLargeFiles::severalMegabytesStayResponsive()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("dane.txt"));
    const QByteArray bytes = makeText(100000); // ok. 5,5 MiB, 100 000 linii
    QVERIFY(writeBytes(path, bytes));
    qInfo("rozmiar pliku: %.1f MiB", double(bytes.size()) / (1024.0 * 1024.0));

    WindowFixture f;
    QElapsedTimer timer;

    timer.start();
    QVERIFY(f.window->openFile(path));
    const qint64 openMs = timer.elapsed();
    QCOMPARE(f.prompts->largeFileQuestions, 0);
    QCOMPARE(f.window->currentEditor()->lines(), 100001);

    SearchOptions options;
    options.pattern = QStringLiteral("kot");
    timer.restart();
    QCOMPARE(f.window->findInAllDocuments(options), 100000);
    const qint64 findMs = timer.elapsed();
    // Panel pokazuje ograniczoną liczbę trafień.
    QVERIFY(f.window->searchResults()->tree()->topLevelItem(0)->childCount() <= 5000);

    timer.restart();
    QCOMPARE(f.window->replaceAllInCurrent(options, QStringLiteral("żbik")), 100000);
    const qint64 replaceMs = timer.elapsed();

    timer.restart();
    f.window->currentEditor()->undo();
    const qint64 undoMs = timer.elapsed();
    QVERIFY(f.window->currentEditor()->text().toUtf8() == bytes);

    timer.restart();
    trigger(*f.window, "actionSave");
    const qint64 saveMs = timer.elapsed();
    QCOMPARE(readBytes(path), bytes);

    qInfo("otwarcie %lld ms, znajdź we wszystkich %lld ms, zamień wszystkie %lld ms, "
          "cofnij %lld ms, zapis %lld ms",
          openMs, findMs, replaceMs, undoMs, saveMs);
    for (const qint64 ms : {openMs, findMs, replaceMs, undoMs, saveMs})
        QVERIFY2(ms < GenerousLimitMs, qPrintable(QString::number(ms)));
}

QTEST_MAIN(TestLargeFiles)
#include "tst_large_files.moc"
