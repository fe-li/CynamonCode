// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "editor/EditorWidget.h"

#include "editor/SyntaxHighlighter.h"
#include "services/Indentation.h"
#include "services/TextPositions.h"

#include <Qsci/qscicommand.h>
#include <Qsci/qscicommandset.h>
#include <Qsci/qscilexer.h>
#include <Qsci/qscilexerhtml.h>

#include <QFontDatabase>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace cynamoncode {

namespace {
constexpr int LineNumberMargin = 0;
constexpr int SymbolMargin = 1;

// Elementy puste HTML — nie mają znacznika zamykającego, więc nie otwierają bloku.
bool isVoidHtmlElement(const QString &name)
{
    static const QSet<QString> elements = {
        QStringLiteral("area"),  QStringLiteral("base"),   QStringLiteral("br"),
        QStringLiteral("col"),   QStringLiteral("embed"),  QStringLiteral("hr"),
        QStringLiteral("img"),   QStringLiteral("input"),  QStringLiteral("link"),
        QStringLiteral("meta"),  QStringLiteral("param"),  QStringLiteral("source"),
        QStringLiteral("track"), QStringLiteral("wbr"),
    };
    return elements.contains(name.toLower());
}

// <nazwa ...>, </nazwa>, <nazwa .../>. Komentarze, <!DOCTYPE> i <?php nie pasują.
const QRegularExpression &tagPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("<(/?)([A-Za-z][A-Za-z0-9:._-]*)[^<>]*?(/?)>"));
    return pattern;
}

const QRegularExpression &closingTagStartPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[ \\t]*</"));
    return pattern;
}

// Pozycja Scintilli (bajty UTF-8) znaku o indeksie `index` w tekście linii.
long bytePosition(long lineStart, const QString &lineText, qsizetype index)
{
    return lineStart + static_cast<long>(lineText.left(index).toUtf8().size());
}

} // namespace

EditorWidget::EditorWidget(QWidget *parent)
    : QsciScintilla(parent)
    , m_font(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    // Bufor w UTF-8 — bez tego QScintilla traktuje tekst jako Latin-1.
    setUtf8(true);
    disableSuggestions();

    setMarginType(LineNumberMargin, QsciScintilla::NumberMargin);
    setMarginLineNumbers(LineNumberMargin, true);
    // Margines symboli jest zbędny: edytor nie wyświetla żadnych znaczników.
    setMarginWidth(SymbolMargin, 0);

    setCaretLineVisible(true);
    setEolMode(QsciScintilla::EolUnix);

    // Zwykłe wcięcia (D024): Enter zachowuje wcięcie; styl „smart” zależy od lexera.
    setAutoIndent(true);
    setTabWidth(Indentation::Width);
    setIndentationWidth(0); // poziom wcięcia = szerokość tabulatora
    setIndentationsUseTabs(false);
    setTabIndents(true);
    setBackspaceUnindents(true);
    setWrapMode(QsciScintilla::WrapNone);

    connect(this, &QsciScintilla::linesChanged, this, &EditorWidget::updateLineNumberMarginWidth);
    setLanguage(LanguageId::Plain);

    for (QsciCommand *command : standardCommands()->commands())
        m_defaultKeyBindings.append({command, command->key(), command->alternateKey()});
}

void EditorWidget::reserveKeys(const QList<int> &keys)
{
    for (const DefaultKeyBinding &binding : std::as_const(m_defaultKeyBindings)) {
        const int key = keys.contains(binding.key) ? 0 : binding.key;
        const int alternateKey = keys.contains(binding.alternateKey) ? 0 : binding.alternateKey;
        if (binding.command->key() != key)
            binding.command->setKey(key);
        if (binding.command->alternateKey() != alternateKey)
            binding.command->setAlternateKey(alternateKey);
    }
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

void EditorWidget::setLanguage(LanguageId language)
{
    QsciLexer *previous = lexer();
    QsciLexer *created = SyntaxHighlighter::createLexer(language, this);
    if (created)
        SyntaxHighlighter::applyPalette(*created, m_palette, m_font);

    setLexer(created);
    delete previous;

    m_language = language;
    applyEditorColors();
}

void EditorWidget::setTheme(const ThemePalette &palette)
{
    m_palette = palette;
    setLanguage(m_language);
}

void EditorWidget::setEditorFont(const QFont &font)
{
    m_font = font;
    setLanguage(m_language); // lexer dostaje czcionkę dla wszystkich stylów
}

void EditorWidget::setLineNumbersVisible(bool visible)
{
    m_lineNumbersVisible = visible;
    updateLineNumberMarginWidth();
}

void EditorWidget::setLexer(QsciLexer *lexer)
{
    QsciScintilla::setLexer(lexer);
    // Lexer nie może przywrócić podpowiedzi (SPECYFIKACJA §1.2).
    disableSuggestions();
}

void EditorWidget::selectRange(const QString &text, qsizetype start, qsizetype length)
{
    const QList<qsizetype> bytes = TextPositions::toUtf8Offsets(text, {start, start + length});
    SendScintilla(QsciScintillaBase::SCI_SETSEL, static_cast<unsigned long>(bytes.at(0)),
                  static_cast<long>(bytes.at(1)));
    SendScintilla(QsciScintillaBase::SCI_SCROLLCARET);
}

std::pair<qsizetype, qsizetype> EditorWidget::selectionRange(const QString &text) const
{
    const long start = SendScintilla(QsciScintillaBase::SCI_GETSELECTIONSTART);
    const long end = SendScintilla(QsciScintillaBase::SCI_GETSELECTIONEND);
    return {TextPositions::toUtf16Index(text, start), TextPositions::toUtf16Index(text, end)};
}

void EditorWidget::applyReplacements(const QString &text, const QList<Replacement> &replacements)
{
    if (replacements.isEmpty())
        return;

    // Wiele zamian: jedna podmiana zakresu od pierwszego do ostatniego trafienia.
    // Tysiące osobnych SCI_REPLACETARGET są w Scintilli bardzo wolne (D039);
    // efekt i jedno cofnięcie pozostają takie same.
    constexpr qsizetype BulkThreshold = 64;
    if (replacements.size() >= BulkThreshold) {
        const qsizetype spanStart = replacements.first().start;
        const qsizetype spanEnd = replacements.last().start + replacements.last().length;
        QString replaced;
        replaced.reserve(spanEnd - spanStart);
        qsizetype position = spanStart;
        for (const Replacement &replacement : replacements) {
            replaced += QStringView(text).mid(position, replacement.start - position);
            replaced += replacement.text;
            position = replacement.start + replacement.length;
        }
        // Zaznaczenie i przewinięcie jak przy osobnych zamianach: pozycje przeliczone
        // przez listę zamian (pozycja wewnątrz trafienia → jego początek).
        const auto [selectionStart, selectionEnd] = selectionRange(text);
        const int firstLine = firstVisibleLine();
        const auto mapPosition = [&replacements](qsizetype position) {
            qsizetype shift = 0;
            for (const Replacement &replacement : replacements) {
                if (replacement.start >= position)
                    break;
                if (position < replacement.start + replacement.length)
                    return replacement.start + shift; // wewnątrz trafienia
                shift += replacement.text.size() - replacement.length;
            }
            return position + shift;
        };
        const qsizetype newSelectionStart = mapPosition(selectionStart);
        const qsizetype newSelectionEnd = mapPosition(selectionEnd);

        const QList<qsizetype> span = TextPositions::toUtf8Offsets(text, {spanStart, spanEnd});
        const QByteArray utf8 = replaced.toUtf8();
        beginUndoAction();
        SendScintilla(QsciScintillaBase::SCI_SETTARGETRANGE, static_cast<unsigned long>(span.at(0)),
                      static_cast<long>(span.at(1)));
        SendScintilla(QsciScintillaBase::SCI_REPLACETARGET, static_cast<unsigned long>(utf8.size()),
                      utf8.constData());
        endUndoAction();

        const QString newText = this->text();
        const QList<qsizetype> selection =
            TextPositions::toUtf8Offsets(newText, {newSelectionStart, newSelectionEnd});
        SendScintilla(QsciScintillaBase::SCI_SETSEL, static_cast<unsigned long>(selection.at(0)),
                      static_cast<long>(selection.at(1)));
        setFirstVisibleLine(firstLine);
        return;
    }

    QList<qsizetype> indices;
    indices.reserve(replacements.size() * 2);
    for (const Replacement &replacement : replacements) {
        indices.append(replacement.start);
        indices.append(replacement.start + replacement.length);
    }
    const QList<qsizetype> bytes = TextPositions::toUtf8Offsets(text, indices);

    beginUndoAction();
    // Od końca — wcześniejsze pozycje bajtowe pozostają ważne.
    for (qsizetype i = replacements.size() - 1; i >= 0; --i) {
        const QByteArray utf8 = replacements.at(i).text.toUtf8();
        SendScintilla(QsciScintillaBase::SCI_SETTARGETRANGE,
                      static_cast<unsigned long>(bytes.at(2 * i)),
                      static_cast<long>(bytes.at(2 * i + 1)));
        SendScintilla(QsciScintillaBase::SCI_REPLACETARGET,
                      static_cast<unsigned long>(utf8.size()), utf8.constData());
    }
    endUndoAction();
}

void EditorWidget::keyPressEvent(QKeyEvent *event)
{
    // Wcięcia wg klamer (D024) rozpoznają „{”/„}” po stylu lexera, a Scintilla
    // koloruje leniwie przy rysowaniu. Przy szybkim pisaniu dokolorowujemy tekst
    // do kursora (przyrostowo, od ostatnio pokolorowanej pozycji).
    colouriseUpTo(SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS));

    // Wcięcia wg znaczników HTML/XML (D024) — po obsłudze klawisza przez QScintillę,
    // w jednej akcji cofania razem ze znakiem.
    const bool isEnter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
    const bool isSlash = event->text() == QLatin1String("/");
    if (autoIndent() && usesTagIndentation() && (isEnter || isSlash)) {
        const int linesBefore = lines();
        beginUndoAction();
        QsciScintilla::keyPressEvent(event);
        if (isEnter && lines() > linesBefore)
            indentAfterOpeningTag();
        else if (isSlash)
            unindentClosingTag();
        endUndoAction();
        return;
    }
    QsciScintilla::keyPressEvent(event);
}

void EditorWidget::colouriseUpTo(long position)
{
    if (!lexer())
        return;
    const long styledEnd = SendScintilla(QsciScintillaBase::SCI_GETENDSTYLED);
    if (styledEnd >= position)
        return;
    // Od początku linii: lexer HTML wznowiony w środku linii dziedziczyłby stan
    // poprzedniego znaku (np. „/>”) i źle oznaczył kolejne znaczniki.
    const long line = SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION,
                                    static_cast<unsigned long>(styledEnd));
    const long lineStart =
        SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, static_cast<unsigned long>(line));
    SendScintilla(QsciScintillaBase::SCI_COLOURISE, static_cast<unsigned long>(lineStart),
                  position);
}

bool EditorWidget::usesTagIndentation() const
{
    return m_language == LanguageId::Html || m_language == LanguageId::Xml
           || m_language == LanguageId::Php;
}

bool EditorWidget::isTagStyleAt(long position)
{
    const long style = SendScintilla(QsciScintillaBase::SCI_GETSTYLEAT,
                                     static_cast<unsigned long>(position));
    // Tylko znaczniki HTML/XML; „<” w komentarzach, napisach, JS i kodzie PHP ma inne style.
    return style == QsciLexerHTML::Tag || style == QsciLexerHTML::UnknownTag;
}

bool EditorWidget::lineOpensTagBlock(int line)
{
    const QString lineText = text(line);
    const long lineStart = SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE,
                                         static_cast<unsigned long>(line));
    const bool html = m_language != LanguageId::Xml;

    // Bilans znaczników w linii: otwarte, a niezamknięte w tej samej linii.
    QStringList open;
    QRegularExpressionMatchIterator it = tagPattern().globalMatch(lineText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (!isTagStyleAt(bytePosition(lineStart, lineText, match.capturedStart())))
            continue;
        const bool closing = !match.captured(1).isEmpty();
        const bool selfClosing = !match.captured(3).isEmpty();
        const QString name = match.captured(2);
        if (closing) {
            if (!open.isEmpty())
                open.removeLast();
            continue;
        }
        if (selfClosing || (html && isVoidHtmlElement(name)))
            continue;
        open.append(name);
    }
    return !open.isEmpty();
}

void EditorWidget::indentAfterOpeningTag()
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    if (line <= 0)
        return;
    // Enter między <div>|</div>: linia z „</div>” zostaje na poziomie otwarcia.
    if (text(line).trimmed().startsWith(QLatin1String("</")))
        return;
    if (!lineOpensTagBlock(line - 1))
        return;

    setIndentation(line, indentation(line - 1) + indentStep());
    SendScintilla(QsciScintillaBase::SCI_GOTOPOS,
                  SendScintilla(QsciScintillaBase::SCI_GETLINEINDENTPOSITION,
                                static_cast<unsigned long>(line)));
}

void EditorWidget::unindentClosingTag()
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    const QString lineText = text(line);
    const QRegularExpressionMatch match = closingTagStartPattern().match(lineText);
    if (!match.hasMatch())
        return;

    // Tylko gdy „</” właśnie wpisano zaraz po wcięciu.
    const long lineStart = SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE,
                                         static_cast<unsigned long>(line));
    const long caret = SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
    if (caret != bytePosition(lineStart, lineText, match.capturedLength()))
        return;

    colouriseUpTo(caret);
    if (!isTagStyleAt(caret - 2))
        return;

    const int current = indentation(line);
    if (current > 0)
        setIndentation(line, std::max(0, current - indentStep()));
}

int EditorWidget::indentStep() const
{
    return indentationWidth() > 0 ? indentationWidth() : tabWidth();
}

void EditorWidget::disableSuggestions()
{
    setAutoCompletionSource(QsciScintilla::AcsNone);
    setAutoCompletionThreshold(-1);
    setCallTipsStyle(QsciScintilla::CallTipsNone);
}

void EditorWidget::applyEditorColors()
{
    if (!lexer()) {
        setPaper(m_palette.background);
        setColor(m_palette.text);
        setFont(m_font);
    }
    setCaretForegroundColor(m_palette.caret);
    setCaretLineBackgroundColor(m_palette.caretLine);
    setSelectionBackgroundColor(m_palette.selection);
    setMarginsBackgroundColor(m_palette.marginBackground);
    setMarginsForegroundColor(m_palette.marginText);
    setMarginsFont(m_font);
    updateLineNumberMarginWidth();
}

void EditorWidget::updateLineNumberMarginWidth()
{
    if (!m_lineNumbersVisible) {
        setMarginWidth(LineNumberMargin, 0);
        return;
    }
    const qsizetype digits = QString::number(std::max(lines(), 1)).size();
    setMarginWidth(LineNumberMargin, QString(digits + 1, QLatin1Char('9')));
}

} // namespace cynamoncode
