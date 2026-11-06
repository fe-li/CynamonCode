// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QList>
#include <QString>

namespace cynamoncode::TextPositions {

// Przeliczanie indeksów QString (UTF-16) na pozycje Scintilli (bajty UTF-8)
// i odwrotnie. `indices` muszą być posortowane rosnąco; jedno przejście po tekście.
QList<qsizetype> toUtf8Offsets(const QString &text, const QList<qsizetype> &indices);

// Indeks UTF-16 znaku zaczynającego się w bajcie `utf8Offset` (lub pierwszego po nim).
qsizetype toUtf16Index(const QString &text, qsizetype utf8Offset);

} // namespace cynamoncode::TextPositions
