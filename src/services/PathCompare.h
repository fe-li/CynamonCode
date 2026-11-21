// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QString>
#include <QtGlobal>

namespace cynamoncode {

// Czy dwie ścieżki (kanoniczne) wskazują ten sam plik. Windows i macOS domyślnie
// nie rozróżniają wielkości liter w nazwach plików — „Plik.txt” i „plik.txt” to
// jeden dokument (jedna karta); Linux rozróżnia.
inline bool samePath(const QString &a, const QString &b)
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

} // namespace cynamoncode
