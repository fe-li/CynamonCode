// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/EncodingService.h"

#include <QHash>

#include <utility>

namespace cynamoncode::EncodingService {

namespace {

const QByteArray &utf8Bom()
{
    static const QByteArray bom("\xEF\xBB\xBF", 3);
    return bom;
}

// Bajty 0x80–0xFF Windows-1250 → znak Unicode; 0 = bajt niezdefiniowany.
// Tablica wygenerowana z kodeka cp1250 (Python) i sprawdzana testem.
constexpr char16_t Windows1250High[128] = {
    0x20AC, 0x0000, 0x201A, 0x0000, 0x201E, 0x2026, 0x2020, 0x2021, // 0x80
    0x0000, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179, // 0x88
    0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 0x90
    0x0000, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A, // 0x98
    0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7, // 0xA0
    0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B, // 0xA8
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7, // 0xB0
    0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C, // 0xB8
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7, // 0xC0
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E, // 0xC8
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7, // 0xD0
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF, // 0xD8
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, // 0xE0
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F, // 0xE8
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7, // 0xF0
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9, // 0xF8
};

// Odwrotna tablica: znak Unicode → bajt (tylko dla zakresu 0x80–0xFF).
const QHash<char16_t, char> &windows1250Reverse()
{
    static const QHash<char16_t, char> reverse = [] {
        QHash<char16_t, char> map;
        for (int i = 0; i < 128; ++i) {
            if (Windows1250High[i] != 0)
                map.insert(Windows1250High[i], static_cast<char>(0x80 + i));
        }
        return map;
    }();
    return reverse;
}

} // namespace

DecodeResult decodeUtf8(const QByteArray &bytes)
{
    DecodeResult result;

    if (bytes.contains('\0')) {
        result.error = DecodeError::ContainsNul;
        return result;
    }

    result.hasBom = bytes.startsWith(utf8Bom());
    const QByteArray payload = result.hasBom ? bytes.mid(utf8Bom().size()) : bytes;

    // fromUtf8 zamienia niepoprawne sekwencje na U+FFFD, więc o poprawności
    // rozstrzyga porównanie bajtów po ponownym zakodowaniu.
    QString text = QString::fromUtf8(payload);
    if (text.toUtf8() != payload) {
        result.error = DecodeError::InvalidUtf8;
        result.hasBom = false;
        return result;
    }

    result.text = std::move(text);
    result.lineEnding = detectLineEnding(payload);
    return result;
}

QByteArray encodeUtf8(const QString &text, bool withBom)
{
    QByteArray out;
    if (withBom)
        out = utf8Bom();
    out += text.toUtf8();
    return out;
}

DecodeResult decodeWindows1250(const QByteArray &bytes)
{
    DecodeResult result;
    if (bytes.contains('\0')) {
        result.error = DecodeError::ContainsNul;
        return result;
    }

    QString text;
    text.reserve(bytes.size());
    for (const char byte : bytes) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x80) {
            text.append(QChar(value));
            continue;
        }
        const char16_t unicode = Windows1250High[value - 0x80];
        if (unicode == 0) {
            result.error = DecodeError::NotWindows1250;
            return result;
        }
        text.append(QChar(unicode));
    }

    // Tablica jest różnowartościowa, ale roundtrip sprawdzamy jak dla UTF-8 (D015).
    const EncodeResult again = encodeWindows1250(text);
    if (!again.ok() || again.bytes != bytes) {
        result.error = DecodeError::NotWindows1250;
        return result;
    }

    result.text = std::move(text);
    result.lineEnding = detectLineEnding(bytes);
    return result;
}

EncodeResult encodeWindows1250(const QString &text)
{
    EncodeResult result;
    result.bytes.reserve(text.size());
    const QHash<char16_t, char> &reverse = windows1250Reverse();
    for (qsizetype i = 0; i < text.size(); ++i) {
        const char16_t unicode = text.at(i).unicode();
        if (unicode < 0x80) {
            result.bytes.append(static_cast<char>(unicode));
            continue;
        }
        const auto found = reverse.constFind(unicode);
        if (found == reverse.cend()) {
            result.bytes.clear();
            result.unencodableAt = i;
            return result;
        }
        result.bytes.append(*found);
    }
    return result;
}

DecodeResult decode(const QByteArray &bytes, TextEncoding encoding)
{
    return encoding == TextEncoding::Windows1250 ? decodeWindows1250(bytes) : decodeUtf8(bytes);
}

EncodeResult encode(const QString &text, TextEncoding encoding, bool withBom)
{
    if (encoding == TextEncoding::Windows1250)
        return encodeWindows1250(text);
    EncodeResult result;
    result.bytes = encodeUtf8(text, withBom);
    return result;
}

LineEnding detectLineEnding(const QByteArray &bytes)
{
    bool hasLf = false;
    bool hasCrLf = false;
    bool hasLoneCr = false;

    const qsizetype size = bytes.size();
    for (qsizetype i = 0; i < size; ++i) {
        const char c = bytes.at(i);
        if (c == '\r') {
            if (i + 1 < size && bytes.at(i + 1) == '\n') {
                hasCrLf = true;
                ++i;
            } else {
                hasLoneCr = true;
            }
        } else if (c == '\n') {
            hasLf = true;
        }
    }

    if (hasLoneCr || (hasLf && hasCrLf))
        return LineEnding::Mixed;
    if (hasCrLf)
        return LineEnding::CRLF;
    if (hasLf)
        return LineEnding::LF;
    return LineEnding::None;
}

} // namespace cynamoncode::EncodingService
