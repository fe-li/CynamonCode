// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/TextPositions.h"

namespace cynamoncode::TextPositions {

namespace {

// Liczba bajtów UTF-8 jednostki UTF-16 na pozycji `i`; para surogatów = 4 bajty
// przypisane pierwszej jednostce (druga = 0).
int utf8Bytes(const QString &text, qsizetype i)
{
    const char16_t unit = text.at(i).unicode();
    if (unit < 0x80)
        return 1;
    if (unit < 0x800)
        return 2;
    if (QChar::isHighSurrogate(unit) && i + 1 < text.size()
        && QChar::isLowSurrogate(text.at(i + 1).unicode()))
        return 4;
    if (QChar::isLowSurrogate(unit) && i > 0 && QChar::isHighSurrogate(text.at(i - 1).unicode()))
        return 0;
    return 3;
}

} // namespace

QList<qsizetype> toUtf8Offsets(const QString &text, const QList<qsizetype> &indices)
{
    QList<qsizetype> result;
    result.reserve(indices.size());
    qsizetype unit = 0;
    qsizetype bytes = 0;
    for (const qsizetype target : indices) {
        while (unit < target && unit < text.size()) {
            bytes += utf8Bytes(text, unit);
            ++unit;
        }
        result.append(bytes);
    }
    return result;
}

qsizetype toUtf16Index(const QString &text, qsizetype utf8Offset)
{
    qsizetype bytes = 0;
    qsizetype unit = 0;
    while (unit < text.size() && bytes < utf8Offset) {
        bytes += utf8Bytes(text, unit);
        ++unit;
    }
    // Nie kończymy w środku pary surogatów.
    if (unit < text.size() && unit > 0 && text.at(unit).isLowSurrogate())
        ++unit;
    return unit;
}

} // namespace cynamoncode::TextPositions
