#pragma once

#include "domain/LanguageId.h"
#include "editor/ThemePalette.h"

#include <QString>

class QFont;
class QObject;
class QsciLexer;

namespace cynamoncode::SyntaxHighlighter {

// Lexer QScintilla dla języka (rodzic Qt: `parent`); nullptr dla zwykłego tekstu.
// Tylko kolorowanie — lexery nie dostają QsciAPIs ani opcji ostrzeżeń.
QsciLexer *createLexer(LanguageId language, QObject *parent);

// Kategoria tokenu na podstawie opisu stylu lexera (D021). Style typu
// „Unclosed string”, „Unknown tag”, „Parsing error” dostają kategorię bazową.
TokenCategory categorize(const QString &styleDescription);

// Nadaje wszystkim stylom lexera kolory palety, wspólną czcionkę i tło motywu,
// wyłącza wypełnianie do końca linii.
void applyPalette(QsciLexer &lexer, const ThemePalette &palette, const QFont &font);

// Największy numer stylu sprawdzany przy nadawaniu kolorów.
inline constexpr int MaxStyle = 255;

} // namespace cynamoncode::SyntaxHighlighter
