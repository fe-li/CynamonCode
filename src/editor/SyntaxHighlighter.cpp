#include "editor/SyntaxHighlighter.h"

#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexerxml.h>

#include <Qsci/qsciscintilla.h>

#include <QFont>

namespace cynamoncode::SyntaxHighlighter {

namespace {

// PHP kolorujemy lexerem HTML, który nie zna klamer. Dla wcięć (D024) „{”/„}”
// w kodzie PHP traktujemy jak początek/koniec bloku; wyłącznie styl operatora PHP,
// więc klamry w HTML i napisach nie zmieniają wcięć.
class PhpLexer final : public QsciLexerHTML
{
public:
    using QsciLexerHTML::QsciLexerHTML;

    const char *blockStart(int *style = nullptr) const override
    {
        if (style)
            *style = QsciLexerHTML::PHPOperator;
        return "{";
    }

    const char *blockEnd(int *style = nullptr) const override
    {
        if (style)
            *style = QsciLexerHTML::PHPOperator;
        return "}";
    }
};

QsciLexer *createLexerOnly(LanguageId language, QObject *parent)
{
    switch (language) {
    case LanguageId::Plain:
        return nullptr;
    case LanguageId::Cpp:
        return new QsciLexerCPP(parent);
    case LanguageId::Python: {
        auto *lexer = new QsciLexerPython(parent);
        // Ostrzeżenia o wcięciach to diagnostyka — zawsze wyłączone.
        lexer->setIndentationWarning(QsciLexerPython::NoWarning);
        return lexer;
    }
    case LanguageId::Java:
        return new QsciLexerJava(parent);
    case LanguageId::JavaScript:
        return new QsciLexerJavaScript(parent);
    case LanguageId::Html:
        return new QsciLexerHTML(parent);
    case LanguageId::Php: // PHP jako fragmenty <?php ... ?> w lexerze HTML (D023)
        return new PhpLexer(parent);
    case LanguageId::Css:
        return new QsciLexerCSS(parent);
    case LanguageId::Sql:
        return new QsciLexerSQL(parent);
    case LanguageId::Json:
        return new QsciLexerJSON(parent);
    case LanguageId::Xml:
        return new QsciLexerXML(parent);
    case LanguageId::Bash:
        return new QsciLexerBash(parent);
    }
    return nullptr;
}

bool hasBraceIndentation(LanguageId language)
{
    switch (language) {
    case LanguageId::Cpp:
    case LanguageId::Java:
    case LanguageId::JavaScript:
    case LanguageId::Css:
    case LanguageId::Php:
        return true;
    case LanguageId::Plain:
    case LanguageId::Python:
    case LanguageId::Html:
    case LanguageId::Sql:
    case LanguageId::Json:
    case LanguageId::Xml:
    case LanguageId::Bash:
        return false;
    }
    return false;
}

} // namespace

QsciLexer *createLexer(LanguageId language, QObject *parent)
{
    QsciLexer *lexer = createLexerOnly(language, parent);
    if (lexer) {
        // 0 = wcięcia „smart” wg klamer lexera; AiMaintain = tylko zachowanie wcięcia (D024).
        lexer->setAutoIndentStyle(hasBraceIndentation(language) ? 0 : QsciScintilla::AiMaintain);
    }
    return lexer;
}

TokenCategory categorize(const QString &styleDescription)
{
    QString d = styleDescription.toLower();
    const auto has = [&d](const char *part) { return d.contains(QLatin1String(part)); };

    // Kod nieaktywny (#if 0) i komentarze — przygaszone.
    if (d.startsWith(QLatin1String("inactive ")) || has("comment") || has("javadoc")
        || has("task marker"))
        return TokenCategory::Comment;

    // Style „błędu” lexera wyglądają jak zwykły tekst (D021).
    if (has("error"))
        return TokenCategory::Default;

    // „Unclosed string” → string, „Unknown tag” → tag itd.
    d.remove(QLatin1String("unclosed "));
    d.remove(QLatin1String("unknown "));

    if (has("other text in a tag"))
        return TokenCategory::Default;
    if (has("pre-processor") || has("@-rule") || has("media rule") || has("decorator")
        || d == QLatin1String("important") || d.startsWith(QLatin1String("start of "))
        || d.startsWith(QLatin1String("end of an xml")) || has("here document delimiter"))
        return TokenCategory::Preprocessor;
    if (has("regular expression") || has("escape") || has("entity") || has("variable")
        || has("scalar") || has("parameter expansion") || has("backticks"))
        return TokenCategory::Special;
    if (has("string") || has("verbatim") || has("here document") || has("cdata")
        || has("value") || has("iri") || has("uuid"))
        return TokenCategory::String;
    if (has("number") || has("literal"))
        return TokenCategory::Number;
    if (has("secondary keyword") || has("global class") || has("class name")
        || has("function or method") || has("selector") || has("pseudo")
        || d.startsWith(QLatin1String("user defined")) || has("sql*plus")
        || has("json-ld keyword"))
        return TokenCategory::Type;
    if (has("keyword") || has("sgml command"))
        return TokenCategory::Keyword;
    if (has("tag"))
        return TokenCategory::Tag;
    if (has("attribute") || has("property") || has("first parameter"))
        return TokenCategory::Attribute;
    if (has("operator") || has("symbol"))
        return TokenCategory::Operator;
    return TokenCategory::Default;
}

void applyPalette(QsciLexer &lexer, const ThemePalette &palette, const QFont &font)
{
    lexer.setDefaultFont(font);
    lexer.setDefaultColor(palette.text);
    lexer.setDefaultPaper(palette.background);

    for (int style = 0; style <= MaxStyle; ++style) {
        const QString description = lexer.description(style);
        if (description.isEmpty())
            continue;
        lexer.setFont(font, style);
        lexer.setPaper(palette.background, style);
        lexer.setEolFill(false, style);
        lexer.setColor(palette.color(categorize(description)), style);
    }
}

} // namespace cynamoncode::SyntaxHighlighter
