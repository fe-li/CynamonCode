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
