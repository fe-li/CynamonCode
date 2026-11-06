#include "editor/ThemePalette.h"

namespace cynamoncode {

namespace {

void setToken(ThemePalette &palette, TokenCategory category, const char *color)
{
    palette.tokens[static_cast<int>(category)] = QColor(QLatin1String(color));
}

} // namespace

// Własna, ciepła paleta (D022) — bez czerwieni kojarzonej z błędami.
ThemePalette ThemePalette::light()
{
    ThemePalette p;
    p.id = ThemeId::Light;
    p.background = QColor(0xfb, 0xfa, 0xf7);
    p.text = QColor(0x26, 0x24, 0x21);
    p.caret = QColor(0x26, 0x24, 0x21);
    p.caretLine = QColor(0xf3, 0xef, 0xe6);
    p.selection = QColor(0xe7, 0xd9, 0xc4);
    p.marginBackground = QColor(0xf0, 0xed, 0xe6);
    p.marginText = QColor(0x8a, 0x84, 0x78);

    setToken(p, TokenCategory::Default, "#262421");
    setToken(p, TokenCategory::Comment, "#7c7769");
    setToken(p, TokenCategory::Keyword, "#9a4a14");
    setToken(p, TokenCategory::Type, "#1d6a8a");
    setToken(p, TokenCategory::String, "#2f7d32");
    setToken(p, TokenCategory::Number, "#7b4fa0");
    setToken(p, TokenCategory::Operator, "#4a4640");
    setToken(p, TokenCategory::Preprocessor, "#a16207");
    setToken(p, TokenCategory::Tag, "#9a4a14");
    setToken(p, TokenCategory::Attribute, "#1d6a8a");
    setToken(p, TokenCategory::Special, "#a16207");
    return p;
}

ThemePalette ThemePalette::dark()
{
    ThemePalette p;
    p.id = ThemeId::Dark;
    p.background = QColor(0x1f, 0x1c, 0x1a);
    p.text = QColor(0xe6, 0xe1, 0xd9);
    p.caret = QColor(0xf5, 0xf0, 0xe8);
    p.caretLine = QColor(0x2b, 0x27, 0x24);
    p.selection = QColor(0x4a, 0x3b, 0x2e);
    p.marginBackground = QColor(0x26, 0x22, 0x20);
    p.marginText = QColor(0x8a, 0x82, 0x79);

    setToken(p, TokenCategory::Default, "#e6e1d9");
    setToken(p, TokenCategory::Comment, "#968e81");
    setToken(p, TokenCategory::Keyword, "#f0a46c");
    setToken(p, TokenCategory::Type, "#7fc4d8");
    setToken(p, TokenCategory::String, "#a5d6a7");
    setToken(p, TokenCategory::Number, "#d7a6e8");
    setToken(p, TokenCategory::Operator, "#cfc8bd");
    setToken(p, TokenCategory::Preprocessor, "#f2c46d");
    setToken(p, TokenCategory::Tag, "#f0a46c");
    setToken(p, TokenCategory::Attribute, "#7fc4d8");
    setToken(p, TokenCategory::Special, "#f2c46d");
    return p;
}

} // namespace cynamoncode
