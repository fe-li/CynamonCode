// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Kolorowanie składni (E2): tokeny dostają kolory palety (T10), żaden styl
// lexera nie wygląda jak diagnostyka (T11), zmiana języka i motywu nie rusza
// treści ani blokady podpowiedzi (T09, T20). D021–D023.

#include "editor/EditorWidget.h"
#include "editor/SyntaxHighlighter.h"
#include "services/LanguageRegistry.h"

#include <Qsci/qscilexer.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexersql.h>

#include <QFontDatabase>
#include <QtTest>

#include <memory>

using namespace cynamoncode;

#define VERIFY_NO_SUGGESTIONS(editor)                                                    \
    do {                                                                                 \
        QVERIFY((editor).autoCompletionSource() == QsciScintilla::AcsNone);              \
        QVERIFY((editor).autoCompletionThreshold() < 0);                                 \
        QVERIFY((editor).callTipsStyle() == QsciScintilla::CallTipsNone);                \
    } while (false)

namespace {

int styleAt(EditorWidget &editor, int position)
{
    editor.SendScintilla(QsciScintillaBase::SCI_COLOURISE, 0UL, -1L);
    return static_cast<int>(
        editor.SendScintilla(QsciScintillaBase::SCI_GETSTYLEAT, static_cast<unsigned long>(position)));
}

} // namespace

class TestSyntaxHighlighting : public QObject
{
    Q_OBJECT

private slots:
    void categorizesStyleDescriptions_data();
    void categorizesStyleDescriptions();
    void noStyleLooksLikeDiagnostics_data();
    void noStyleLooksLikeDiagnostics();
    void errorLikeStylesUseBaseColors();
    void pythonIndentationWarningsAreOff();
    void tokensAreColored_data();
    void tokensAreColored();
    void languageSwitchKeepsTextCursorAndModified();
    void themeSwitchKeepsTextAndSuggestionsOff();
};

void TestSyntaxHighlighting::categorizesStyleDescriptions_data()
{
    QTest::addColumn<QString>("description");
    QTest::addColumn<int>("expected");
    const auto row = [](const char *description, TokenCategory category) {
        QTest::newRow(description) << QString::fromUtf8(description) << int(category);
    };
    row("Keyword", TokenCategory::Keyword);
    row("C comment", TokenCategory::Comment);
    row("JavaDoc keyword error", TokenCategory::Comment);
    row("Inactive keyword", TokenCategory::Comment);
    row("Double-quoted string", TokenCategory::String);
    row("Unclosed string", TokenCategory::String);
    row("JavaScript unclosed string", TokenCategory::String);
    row("Number", TokenCategory::Number);
    row("Operator", TokenCategory::Operator);
    row("Pre-processor block", TokenCategory::Preprocessor);
    row("Tag", TokenCategory::Tag);
    row("Unknown tag", TokenCategory::Tag);
    row("Attribute", TokenCategory::Attribute);
    row("Unknown attribute", TokenCategory::Attribute);
    row("Unknown property", TokenCategory::Attribute);
    row("Other text in a tag", TokenCategory::Default);
    row("Parsing error", TokenCategory::Default);
    row("SGML error", TokenCategory::Default);
    row("Error", TokenCategory::Default);
    row("PHP double-quoted variable", TokenCategory::Special);
    row("Escape sequence", TokenCategory::Special);
    row("Global classes and typedefs", TokenCategory::Type);
    row("Start of a PHP fragment", TokenCategory::Preprocessor);
    row("Identifier", TokenCategory::Default);
    row("Default", TokenCategory::Default);
}

void TestSyntaxHighlighting::categorizesStyleDescriptions()
{
    QFETCH(QString, description);
    QFETCH(int, expected);
    QCOMPARE(int(SyntaxHighlighter::categorize(description)), expected);
}

void TestSyntaxHighlighting::noStyleLooksLikeDiagnostics_data()
{
    QTest::addColumn<int>("language");
    QTest::addColumn<int>("theme");
    for (const LanguageId language : LanguageRegistry::all()) {
        if (language == LanguageId::Plain)
            continue;
        for (const ThemeId theme : {ThemeId::Light, ThemeId::Dark}) {
            const QByteArray name = LanguageRegistry::key(language).toUtf8()
                                    + (theme == ThemeId::Dark ? "/ciemny" : "/jasny");
            QTest::newRow(name.constData()) << int(language) << int(theme);
        }
    }
}

void TestSyntaxHighlighting::noStyleLooksLikeDiagnostics()
{
    QFETCH(int, language);
    QFETCH(int, theme);
    const ThemePalette palette = ThemePalette::forId(static_cast<ThemeId>(theme));
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    std::unique_ptr<QsciLexer> lexer(
        SyntaxHighlighter::createLexer(static_cast<LanguageId>(language), nullptr));
    QVERIFY(lexer);
    QVERIFY(lexer->apis() == nullptr);
    SyntaxHighlighter::applyPalette(*lexer, palette, font);

    const QList<QColor> allowed = palette.tokenColors();
    int checked = 0;
    for (int style = 0; style <= SyntaxHighlighter::MaxStyle; ++style) {
        const QString description = lexer->description(style);
        if (description.isEmpty())
            continue;
        ++checked;
        const QByteArray what = QByteArray::number(style) + ' ' + description.toUtf8();
        QVERIFY2(allowed.contains(lexer->color(style)), what.constData());
        QVERIFY2(lexer->paper(style) == palette.background, what.constData());
        QVERIFY2(!lexer->eolFill(style), what.constData());
        QVERIFY2(!lexer->font(style).underline(), what.constData());
        QVERIFY2(!lexer->font(style).strikeOut(), what.constData());
    }
    QVERIFY(checked > 5);
}

void TestSyntaxHighlighting::errorLikeStylesUseBaseColors()
{
    const ThemePalette palette = ThemePalette::light();
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const auto lexerFor = [&](LanguageId language) {
        std::unique_ptr<QsciLexer> lexer(SyntaxHighlighter::createLexer(language, nullptr));
        SyntaxHighlighter::applyPalette(*lexer, palette, font);
        return lexer;
    };

    const auto cpp = lexerFor(LanguageId::Cpp);
    QCOMPARE(cpp->color(QsciLexerCPP::UnclosedString), palette.color(TokenCategory::String));
    const auto python = lexerFor(LanguageId::Python);
    QCOMPARE(python->color(QsciLexerPython::UnclosedString), palette.color(TokenCategory::String));
    const auto html = lexerFor(LanguageId::Html);
    QCOMPARE(html->color(QsciLexerHTML::UnknownTag), html->color(QsciLexerHTML::Tag));
    QCOMPARE(html->color(QsciLexerHTML::UnknownAttribute), html->color(QsciLexerHTML::Attribute));
    const auto css = lexerFor(LanguageId::Css);
    QCOMPARE(css->color(QsciLexerCSS::UnknownProperty), css->color(QsciLexerCSS::CSS1Property));
    const auto json = lexerFor(LanguageId::Json);
    QCOMPARE(json->color(QsciLexerJSON::Error), palette.text);
    const auto bash = lexerFor(LanguageId::Bash);
    QCOMPARE(bash->color(QsciLexerBash::Error), palette.text);
}

void TestSyntaxHighlighting::pythonIndentationWarningsAreOff()
{
    std::unique_ptr<QsciLexer> lexer(SyntaxHighlighter::createLexer(LanguageId::Python, nullptr));
    auto *python = qobject_cast<QsciLexerPython *>(lexer.get());
    QVERIFY(python);
    QVERIFY(python->indentationWarning() == QsciLexerPython::NoWarning);
}

void TestSyntaxHighlighting::tokensAreColored_data()
{
    QTest::addColumn<int>("language");
    QTest::addColumn<QString>("text");
    QTest::addColumn<int>("position");
    QTest::addColumn<int>("expectedStyle");
    QTest::addColumn<int>("expectedCategory");

    const auto row = [](const char *name, LanguageId language, const char *text, int position,
                        int style, TokenCategory category) {
        QTest::newRow(name) << int(language) << QString::fromUtf8(text) << position << style
                            << int(category);
    };
    row("cpp", LanguageId::Cpp, "int main() { return 0; }", 0, QsciLexerCPP::Keyword,
        TokenCategory::Keyword);
    row("cpp komentarz", LanguageId::Cpp, "x; // uwaga", 5, QsciLexerCPP::CommentLine,
        TokenCategory::Comment);
    row("python", LanguageId::Python, "def f():\n    pass\n", 0, QsciLexerPython::Keyword,
        TokenCategory::Keyword);
    row("java", LanguageId::Java, "class A {}", 0, QsciLexerCPP::Keyword, TokenCategory::Keyword);
    row("javascript", LanguageId::JavaScript, "var x = 1;", 0, QsciLexerCPP::Keyword,
        TokenCategory::Keyword);
    row("html", LanguageId::Html, "<html><body></body></html>", 1, QsciLexerHTML::Tag,
        TokenCategory::Tag);
    row("css", LanguageId::Css, "a { color: blue; }", 4, QsciLexerCSS::CSS1Property,
        TokenCategory::Attribute);
    row("sql", LanguageId::Sql, "select 1;", 0, QsciLexerSQL::Keyword, TokenCategory::Keyword);
    row("json", LanguageId::Json, "{\"a\": 1}", 1, QsciLexerJSON::Property,
        TokenCategory::Attribute);
    row("xml", LanguageId::Xml, "<a b=\"1\"/>", 1, QsciLexerHTML::Tag, TokenCategory::Tag);
    row("bash", LanguageId::Bash, "if true; then echo hi; fi", 0, QsciLexerBash::Keyword,
        TokenCategory::Keyword);
    row("php", LanguageId::Php, "<?php $x = 1; ?>", 6, QsciLexerHTML::PHPVariable,
        TokenCategory::Special);
}

void TestSyntaxHighlighting::tokensAreColored()
{
    QFETCH(int, language);
    QFETCH(QString, text);
    QFETCH(int, position);
    QFETCH(int, expectedStyle);
    QFETCH(int, expectedCategory);

    EditorWidget editor;
    editor.setLanguage(static_cast<LanguageId>(language));
    QVERIFY(editor.loadText(text));

    QCOMPARE(styleAt(editor, position), expectedStyle);
    QVERIFY(editor.lexer());
    QCOMPARE(editor.lexer()->color(expectedStyle),
             editor.theme().color(static_cast<TokenCategory>(expectedCategory)));
    VERIFY_NO_SUGGESTIONS(editor);
}

void TestSyntaxHighlighting::languageSwitchKeepsTextCursorAndModified()
{
    EditorWidget editor;
    QVERIFY(editor.loadText(QStringLiteral("int a = 1;\nint b = 2;\n")));
    editor.setCursorPosition(1, 4);
    editor.insert(QStringLiteral("x"));
    QVERIFY(editor.isModified());
    const QString text = editor.text();

    for (const LanguageId language : {LanguageId::Cpp, LanguageId::Python, LanguageId::Plain,
                                      LanguageId::Html, LanguageId::Cpp}) {
        editor.setLanguage(language);
        QCOMPARE(int(editor.language()), int(language));
        QCOMPARE(editor.text(), text);
        QVERIFY(editor.isModified());
        int line = -1;
        int index = -1;
        editor.getCursorPosition(&line, &index);
        QCOMPARE(line, 1);
        QCOMPARE(index, 4);
        VERIFY_NO_SUGGESTIONS(editor);
    }
    QVERIFY(editor.lexer());
    QVERIFY(editor.isUndoAvailable());
}

void TestSyntaxHighlighting::themeSwitchKeepsTextAndSuggestionsOff()
{
    EditorWidget editor;
    editor.setLanguage(LanguageId::Python);
    QVERIFY(editor.loadText(QStringLiteral("def f():\n    return 1\n")));

    editor.setTheme(ThemePalette::dark());
    QVERIFY(editor.theme().id == ThemeId::Dark);
    QCOMPARE(editor.lexer()->paper(QsciLexerPython::Default), ThemePalette::dark().background);
    QCOMPARE(editor.lexer()->color(QsciLexerPython::Keyword),
             ThemePalette::dark().color(TokenCategory::Keyword));
    QCOMPARE(int(editor.language()), int(LanguageId::Python));
    QCOMPARE(editor.text(), QStringLiteral("def f():\n    return 1\n"));
    QVERIFY(!editor.isModified());
    VERIFY_NO_SUGGESTIONS(editor);

    editor.setTheme(ThemePalette::light());
    QCOMPARE(editor.lexer()->paper(QsciLexerPython::Default), ThemePalette::light().background);
    VERIFY_NO_SUGGESTIONS(editor);
}

QTEST_MAIN(TestSyntaxHighlighting)
#include "tst_syntax_highlighting.moc"
