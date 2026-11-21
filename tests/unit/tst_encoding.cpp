// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// EncodingService: bezstratny roundtrip UTF-8, BOM, odmowa NUL i niepoprawnych
// sekwencji, rozpoznawanie końców linii (TESTY.md T12–T15, zakres E1).

#include "services/EncodingService.h"

#include <QStringDecoder>
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
    void windows1250AllDefinedBytesRoundtrip();
    void windows1250PolishLetters();
    void windows1250RejectsUndefinedBytesAndNul();
    void windows1250ReportsUnencodableCharacter();
    void windows1250MatchesSystemCodecWhereAvailable();
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

void TestEncoding::windows1250AllDefinedBytesRoundtrip()
{
    // Każdy bajt poza NUL i pięcioma niezdefiniowanymi daje znak i wraca bez zmian.
    QByteArray all;
    for (int b = 1; b < 256; ++b) {
        if (b != 0x81 && b != 0x83 && b != 0x88 && b != 0x90 && b != 0x98)
            all.append(static_cast<char>(b));
    }
    QCOMPARE(all.size(), 250);
    const DecodeResult decoded = EncodingService::decodeWindows1250(all);
    QVERIFY(decoded.ok());
    QCOMPARE(decoded.text.size(), 250);
    const EncodeResult encoded = EncodingService::encodeWindows1250(decoded.text);
    QVERIFY(encoded.ok());
    QCOMPARE(encoded.bytes, all);
    QVERIFY(!decoded.text.contains(QChar::ReplacementCharacter));
}

void TestEncoding::windows1250PolishLetters()
{
    const QString letters = QStringLiteral("ąćęłńóśźż ĄĆĘŁŃÓŚŹŻ €");
    const QByteArray bytes("\xB9\xE6\xEA\xB3\xF1\xF3\x9C\x9F\xBF \xA5\xC6\xCA\xA3\xD1\xD3\x8C\x8F\xAF \x80");
    QCOMPARE(EncodingService::encodeWindows1250(letters).bytes, bytes);
    const DecodeResult decoded = EncodingService::decodeWindows1250(bytes + "\r\n");
    QVERIFY(decoded.ok());
    QCOMPARE(decoded.text, letters + QStringLiteral("\r\n"));
    QCOMPARE(decoded.lineEnding, LineEnding::CRLF);
    QVERIFY(!decoded.hasBom);
    // Ten sam plik nie jest poprawnym UTF-8 — dlatego potrzebne jest pytanie.
    QCOMPARE(EncodingService::decodeUtf8(bytes).error, DecodeError::InvalidUtf8);
}

void TestEncoding::windows1250RejectsUndefinedBytesAndNul()
{
    for (const char undefined : {'\x81', '\x83', '\x88', '\x90', '\x98'}) {
        const QByteArray bytes = QByteArray("abc") + undefined + "def";
        QCOMPARE(EncodingService::decodeWindows1250(bytes).error, DecodeError::NotWindows1250);
    }
    QCOMPARE(EncodingService::decodeWindows1250(QByteArray("a\0b", 3)).error,
             DecodeError::ContainsNul);
}

void TestEncoding::windows1250ReportsUnencodableCharacter()
{
    const EncodeResult kot = EncodingService::encodeWindows1250(QStringLiteral("Kot 🐈 i ąę"));
    QVERIFY(!kot.ok());
    QCOMPARE(kot.unencodableAt, qsizetype(4));
    QVERIFY(kot.bytes.isEmpty());
    // Litera spoza Europy Środkowej (np. francuskie „è”) też nie istnieje w CP1250.
    QCOMPARE(EncodingService::encodeWindows1250(QStringLiteral("très")).unencodableAt,
             qsizetype(2));
    QVERIFY(EncodingService::encodeWindows1250(QStringLiteral("zażółć")).ok());
    // Wspólne API: UTF-8 zapisuje wszystko.
    QVERIFY(EncodingService::encode(QStringLiteral("🐈"), TextEncoding::Utf8, false).ok());
}

void TestEncoding::windows1250MatchesSystemCodecWhereAvailable()
{
    // Na Linuksie Qt ma zwykle ICU — tablicę sprawdzamy też z kodekiem systemu.
    QStringDecoder system("windows-1250");
    if (!system.isValid())
        QSKIP("Brak kodeka windows-1250 w tej kompilacji Qt (bez ICU).");
    QByteArray all;
    for (int b = 0x20; b < 256; ++b) {
        if (b != 0x81 && b != 0x83 && b != 0x88 && b != 0x90 && b != 0x98)
            all.append(static_cast<char>(b));
    }
    const QString expected = system(all);
    QCOMPARE(EncodingService::decodeWindows1250(all).text, expected);
}

QTEST_GUILESS_MAIN(TestEncoding)
#include "tst_encoding.moc"
