// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

namespace cynamoncode {

// Rozpoznany styl końców linii w pliku.
// Mixed obejmuje też samotne CR (stary format Mac) — osobna obsługa w E2.
enum class LineEnding {
    None,  // brak końców linii (pusty plik lub jedna linia)
    LF,
    CRLF,
    Mixed,
};

} // namespace cynamoncode
