// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QString>
#include <QtGlobal>

namespace cynamoncode {

struct SearchOptions
{
    QString pattern;
    bool caseSensitive = false;
    bool wholeWords = false;
    bool regex = false; // składnia PCRE (QRegularExpression), D028
    bool wrap = true;
};

// Pozycje w indeksach QString (UTF-16) tekstu, w którym szukano.
struct SearchMatch
{
    qsizetype start = 0;
    qsizetype length = 0;
};

struct Replacement
{
    qsizetype start = 0;
    qsizetype length = 0;
    QString text;
};

} // namespace cynamoncode
