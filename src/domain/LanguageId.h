#pragma once

namespace cynamoncode {

// Języki kolorowane w E2 (pierwsza fala, SPECYFIKACJA §1.3). Wyłącznie
// kolorowanie składni — żadnej analizy poprawności.
enum class LanguageId {
    Plain,
    Cpp,
    Python,
    Java,
    JavaScript,
    Html,
    Css,
    Php,
    Sql,
    Json,
    Xml,
    Bash,
};

} // namespace cynamoncode
