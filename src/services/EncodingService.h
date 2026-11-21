// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "domain/LineEnding.h"
#include "domain/TextEncoding.h"

#include <QByteArray>
#include <QString>

namespace cynamoncode {

enum class DecodeError {
    None,
    ContainsNul, // bajty NUL — plik binarny albo kodowanie inne niż UTF-8
    InvalidUtf8, // bajty nie dają się odczytać i zapisać z powrotem bez zmian
    NotWindows1250, // bajt bez znaczenia w Windows-1250 (0x81, 0x83, 0x88, 0x90, 0x98)
};

struct DecodeResult
{
    DecodeError error = DecodeError::None;
    QString text;
    bool hasBom = false;
    LineEnding lineEnding = LineEnding::None;

    bool ok() const { return error == DecodeError::None; }
};

struct EncodeResult
{
    QByteArray bytes;
    qsizetype unencodableAt = -1; // pozycja (QString) pierwszego znaku spoza kodowania

    bool ok() const { return unencodableAt < 0; }
};

namespace EncodingService {

// Dekoduje UTF-8 (z BOM lub bez). Wynik jest akceptowany tylko wtedy, gdy
// encodeUtf8(text, hasBom) odtwarza dokładnie te same bajty (D015) — nigdy
// nie podstawia po cichu U+FFFD.
DecodeResult decodeUtf8(const QByteArray &bytes);

QByteArray encodeUtf8(const QString &text, bool withBom);

// Windows-1250 (D051): własna tablica, bez zależności od ICU. Dekodowanie odmawia
// bajtów NUL i bajtów niezdefiniowanych; kodowanie zgłasza pierwszy znak spoza
// kodowania (nigdy nie zastępuje go po cichu).
DecodeResult decodeWindows1250(const QByteArray &bytes);
EncodeResult encodeWindows1250(const QString &text);

DecodeResult decode(const QByteArray &bytes, TextEncoding encoding);
EncodeResult encode(const QString &text, TextEncoding encoding, bool withBom);

LineEnding detectLineEnding(const QByteArray &bytes);

} // namespace EncodingService

} // namespace cynamoncode
