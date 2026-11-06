#pragma once

#include <Qsci/qsciscintilla.h>

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

    void setLexer(QsciLexer *lexer = nullptr) override;

private:
    void disableSuggestions();
    void updateLineNumberMarginWidth();
};

} // namespace cynamoncode
