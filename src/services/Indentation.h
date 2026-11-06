// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QString>

namespace cynamoncode::Indentation {

// Czy istniejący tekst jest wcięty tabulatorami (D024). Liczy linie zaczynające
// się tabulatorem oraz co najmniej dwiema spacjami (pojedyncza spacja, np. „ *”
// w komentarzach, nie rozstrzyga). Bez wcięć lub przy remisie → false (spacje).
bool usesTabs(const QString &text);

// Szerokość tabulatora i poziomu wcięcia w spacjach.
inline constexpr int Width = 4;

} // namespace cynamoncode::Indentation
