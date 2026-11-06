#pragma once

#include "domain/LineEnding.h"

#include <QByteArray>
#include <QString>

namespace cynamoncode {

enum class DecodeError {
    None,
    ContainsNul, // bajty NUL — plik binarny albo kodowanie inne niż UTF-8
    InvalidUtf8, // bajty nie dają się odczytać i zapisać z powrotem bez zmian
};

struct DecodeResult
{
    DecodeError error = DecodeError::None;
    QString text;
    bool hasBom = false;
    LineEnding lineEnding = LineEnding::None;

    bool ok() const { return error == DecodeError::None; }
};

namespace EncodingService {

// Dekoduje UTF-8 (z BOM lub bez). Wynik jest akceptowany tylko wtedy, gdy
// encodeUtf8(text, hasBom) odtwarza dokładnie te same bajty (D015) — nigdy
// nie podstawia po cichu U+FFFD.
DecodeResult decodeUtf8(const QByteArray &bytes);

QByteArray encodeUtf8(const QString &text, bool withBom);

LineEnding detectLineEnding(const QByteArray &bytes);

} // namespace EncodingService

} // namespace cynamoncode
