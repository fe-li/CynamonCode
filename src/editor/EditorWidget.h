#pragma once

#include "domain/LanguageId.h"
#include "editor/ThemePalette.h"

#include <Qsci/qsciscintilla.h>

#include <QFont>

class QsciLexer;

namespace cynamoncode {

// Pojedynczy bufor edycji. Każda instancja ma wyłączone autouzupełnianie
// i call tips — w konstruktorze oraz po każdej zmianie lexera (D013).
// Nie zapisuje plików i nie zna sesji.
class EditorWidget final : public QsciScintilla
{
    Q_OBJECT

public:
    explicit EditorWidget(QWidget *parent = nullptr);

    // Wczytuje tekst jako stan „zapisany” (bez historii cofania).
    // Zwraca false, jeśli bufor nie zawiera dokładnie podanego tekstu.
    bool loadText(const QString &text);

    // Zmienia tylko kolorowanie; treść, kursor i stan modyfikacji zostają.
    void setLanguage(LanguageId language);
    LanguageId language() const { return m_language; }

    void setTheme(const ThemePalette &palette);
    const ThemePalette &theme() const { return m_palette; }

    void setLexer(QsciLexer *lexer = nullptr) override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void disableSuggestions();
    void colouriseUpTo(long position);
    bool usesTagIndentation() const;
    bool isTagStyleAt(long position);
    bool lineOpensTagBlock(int line);
    void indentAfterOpeningTag();
    void unindentClosingTag();
    int indentStep() const;
    void applyEditorColors();
    void updateLineNumberMarginWidth();

    LanguageId m_language = LanguageId::Plain;
    ThemePalette m_palette = ThemePalette::light();
    QFont m_font;
};

} // namespace cynamoncode
