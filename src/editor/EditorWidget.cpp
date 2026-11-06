#include "editor/EditorWidget.h"

#include <QFontDatabase>

#include <algorithm>

namespace cynamoncode {

namespace {
constexpr int LineNumberMargin = 0;
constexpr int SymbolMargin = 1;
} // namespace

EditorWidget::EditorWidget(QWidget *parent)
    : QsciScintilla(parent)
{
    // Bufor w UTF-8 — bez tego QScintilla traktuje tekst jako Latin-1.
    setUtf8(true);
    disableSuggestions();

    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setFont(font);
    setMarginsFont(font);

    setMarginType(LineNumberMargin, QsciScintilla::NumberMargin);
    setMarginLineNumbers(LineNumberMargin, true);
    // Margines symboli jest zbędny: edytor nie wyświetla żadnych znaczników.
    setMarginWidth(SymbolMargin, 0);

    setCaretLineVisible(true);
    setEolMode(QsciScintilla::EolUnix);
    setWrapMode(QsciScintilla::WrapNone);

    connect(this, &QsciScintilla::linesChanged, this, &EditorWidget::updateLineNumberMarginWidth);
    updateLineNumberMarginWidth();
}

bool EditorWidget::loadText(const QString &text)
{
    setText(text);
    if (this->text() != text)
        return false;
    SendScintilla(QsciScintillaBase::SCI_EMPTYUNDOBUFFER);
    setModified(false);
    return true;
}

void EditorWidget::setLexer(QsciLexer *lexer)
{
    QsciScintilla::setLexer(lexer);
    // Lexer nie może przywrócić podpowiedzi (CLAUDE.md).
    disableSuggestions();
}

void EditorWidget::disableSuggestions()
{
    setAutoCompletionSource(QsciScintilla::AcsNone);
    setAutoCompletionThreshold(-1);
    setCallTipsStyle(QsciScintilla::CallTipsNone);
}

void EditorWidget::updateLineNumberMarginWidth()
{
    const qsizetype digits = QString::number(std::max(lines(), 1)).size();
    setMarginWidth(LineNumberMargin, QString(digits + 1, QLatin1Char('9')));
}

} // namespace cynamoncode
