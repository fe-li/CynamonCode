// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "domain/LanguageId.h"
#include "domain/SearchOptions.h"
#include "editor/ThemePalette.h"

#include <QList>

#include <utility>

#include <Qsci/qsciscintilla.h>

#include <QFont>

class QsciCommand;
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

    // Czcionka tekstu i marginesów (także wszystkich stylów lexera).
    void setEditorFont(const QFont &font);
    QFont editorFont() const { return m_font; }

    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const { return m_lineNumbersVisible; }

    void setLexer(QsciLexer *lexer = nullptr) override;

    // Klawisze przypisane poleceniom okna (D045, wartości QKeyCombination::toCombined)
    // są zdejmowane z wbudowanej mapy klawiszy QScintilli, żeby skrót okna działał
    // także w edytorze; pozostałe polecenia edytora wracają do domyślnych klawiszy.
    void reserveKeys(const QList<int> &keys);

    // Wyszukiwanie (D028). `text` to bieżący tekst edytora (text()), a pozycje
    // są indeksami QString; przeliczenie na bajty Scintilli odbywa się tutaj.
    void selectRange(const QString &text, qsizetype start, qsizetype length);
    std::pair<qsizetype, qsizetype> selectionRange(const QString &text) const;
    // Wszystkie zamiany jako jedna akcja cofania (T17).
    void applyReplacements(const QString &text, const QList<Replacement> &replacements);

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
    bool m_lineNumbersVisible = true;

    struct DefaultKeyBinding
    {
        QsciCommand *command = nullptr; // własność QsciScintilla
        int key = 0;
        int alternateKey = 0;
    };
    QList<DefaultKeyBinding> m_defaultKeyBindings;
};

} // namespace cynamoncode
