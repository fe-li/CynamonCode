#pragma once

#include <QColor>
#include <QList>

#include <array>

namespace cynamoncode {

enum class ThemeId {
    Light,
    Dark,
};

// Kategorie tokenów (D021). Celowo nie ma kategorii „błąd” — żaden styl lexera
// nie może wyglądać jak diagnostyka.
enum class TokenCategory {
    Default,
    Comment,
    Keyword,
    Type,
    String,
    Number,
    Operator,
    Preprocessor,
    Tag,
    Attribute,
    Special,
};
inline constexpr int TokenCategoryCount = 11;

struct ThemePalette
{
    ThemeId id = ThemeId::Light;
    QColor background;
    QColor text;
    QColor caret;
    QColor caretLine;
    QColor selection;
    QColor marginBackground;
    QColor marginText;
    std::array<QColor, TokenCategoryCount> tokens;

    QColor color(TokenCategory category) const { return tokens[static_cast<int>(category)]; }
    QList<QColor> tokenColors() const { return QList<QColor>(tokens.begin(), tokens.end()); }

    static ThemePalette light();
    static ThemePalette dark();
    static ThemePalette forId(ThemeId id) { return id == ThemeId::Dark ? dark() : light(); }
};

} // namespace cynamoncode
