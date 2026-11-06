// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/Indentation.h"

namespace cynamoncode::Indentation {

bool usesTabs(const QString &text)
{
    qsizetype tabLines = 0;
    qsizetype spaceLines = 0;
    bool atLineStart = true;

    const qsizetype size = text.size();
    for (qsizetype i = 0; i < size; ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            atLineStart = true;
            continue;
        }
        if (!atLineStart)
            continue;
        atLineStart = false;

        if (c == QLatin1Char('\t'))
            ++tabLines;
        else if (c == QLatin1Char(' ') && i + 1 < size && text.at(i + 1) == QLatin1Char(' '))
            ++spaceLines;
    }
    return tabLines > spaceLines;
}

} // namespace cynamoncode::Indentation
