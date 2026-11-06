// EncodingService: bezstratny roundtrip UTF-8, BOM, odmowa NUL i niepoprawnych
// sekwencji, rozpoznawanie końców linii (TESTY.md T12–T15, zakres E1).

#include "services/EncodingService.h"

#include <QtTest>

using namespace cynamoncode;

class TestEncoding : public QObject
{
    Q_OBJECT

private slots:
    void roundtripKeepsBytes_data();
    void roundtripKeepsBytes();
    void detectsAndKeepsBom();
    void rejectsNul();
    void rejectsInvalidUtf8_data();
    void rejectsInvalidUtf8();
    void doubleBomNeverChangesSilently();
    void detectsLineEndings_data();
    void detectsLineEndings();
};

void TestEncoding::roundtripKeepsBytes_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("pusty") << QByteArray();
    QTest::newRow("ascii") << QByteArray("int main() { return 0; }\n");
    QTest::newRow("polskie znaki") << QByteArray("Zażółć gęślą jaźń\nŻÓŁW ąęćłńóśźż\n");
    QTest::newRow("emoji") << QByteArray("Kot 🐈 i ogień 🔥\n");
    QTest::newRow("crlf") << QByteArray("linia 1\r\nlinia 2\r\n");
    QTest::newRow("mieszane") << QByteArray("a\nb\r\nc\rd");
    QTest::newRow("U+FFFD w pliku") << QByteArray("znak zastępczy: \xEF\xBF\xBD koniec");
}

void TestEncoding::roundtripKeepsBytes()
{
    QFETCH(QByteArray, bytes);
    const DecodeResult decoded = EncodingService::decodeUtf8(bytes);
    QVERIFY(decoded.ok());
    QVERIFY(!decoded.hasBom);
    QCOMPARE(EncodingService::encodeUtf8(decoded.text, decoded.hasBom), bytes);
}

void TestEncoding::detectsAndKeepsBom()
{
    const QByteArray bytes = QByteArray("\xEF\xBB\xBF", 3) + QByteArray("Zażółć\r\n");
    const DecodeResult decoded = EncodingService::decodeUtf8(bytes);
    QVERIFY(decoded.ok());
    QVERIFY(decoded.hasBom);
    QCOMPARE(decoded.text, QStringLiteral("Zażółć\r\n"));
    QCOMPARE(EncodingService::encodeUtf8(decoded.text, true), bytes);
}

void TestEncoding::rejectsNul()
{
    const QByteArray bytes("tekst\0binarny", 13);
    const DecodeResult decoded = EncodingService::decodeUtf8(bytes);
    QVERIFY(decoded.error == DecodeError::ContainsNul);
    QVERIFY(decoded.text.isEmpty());
}

void TestEncoding::rejectsInvalidUtf8_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::newRow("Latin-1") << QByteArray("za\xBF\xF3\xB3" "\xE6");
    QTest::newRow("urwana sekwencja") << QByteArray("abc\xC5");
    QTest::newRow("zła kontynuacja") << QByteArray("\xC3\x28");
    QTest::newRow("overlong") << QByteArray("\xC0\xAF");
    QTest::newRow("surogat CESU-8") << QByteArray("\xED\xA0\x80");
    QTest::newRow("bajt 0xFF") << QByteArray("\xFF\xFE" "abc");
}

void TestEncoding::rejectsInvalidUtf8()
{
    QFETCH(QByteArray, bytes);
    const DecodeResult decoded = EncodingService::decodeUtf8(bytes);
    QVERIFY(decoded.error == DecodeError::InvalidUtf8);
    QVERIFY(decoded.text.isEmpty());
}

void TestEncoding::doubleBomNeverChangesSilently()
{
    const QByteArray bom("\xEF\xBB\xBF", 3);
    const QByteArray bytes = bom + bom + QByteArray("tekst");
    const DecodeResult decoded = EncodingService::decodeUtf8(bytes);
    // Dopuszczalne są dwa wyniki: bezstratne otwarcie albo odmowa. Nigdy cicha zmiana bajtów.
    if (decoded.ok())
        QCOMPARE(EncodingService::encodeUtf8(decoded.text, decoded.hasBom), bytes);
    else
        QVERIFY(decoded.error == DecodeError::InvalidUtf8);
}

void TestEncoding::detectsLineEndings_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<int>("expected");
    QTest::newRow("brak") << QByteArray("jedna linia") << int(LineEnding::None);
    QTest::newRow("pusty") << QByteArray() << int(LineEnding::None);
    QTest::newRow("lf") << QByteArray("a\nb\n") << int(LineEnding::LF);
    QTest::newRow("crlf") << QByteArray("a\r\nb\r\n") << int(LineEnding::CRLF);
    QTest::newRow("lf+crlf") << QByteArray("a\nb\r\n") << int(LineEnding::Mixed);
    QTest::newRow("samotne cr") << QByteArray("a\rb\r") << int(LineEnding::Mixed);
    QTest::newRow("cr na końcu") << QByteArray("a\r\nb\r") << int(LineEnding::Mixed);
}

void TestEncoding::detectsLineEndings()
{
    QFETCH(QByteArray, bytes);
    QFETCH(int, expected);
    QCOMPARE(int(EncodingService::detectLineEnding(bytes)), expected);
}

QTEST_GUILESS_MAIN(TestEncoding)
#include "tst_encoding.moc"
