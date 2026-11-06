// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Automatyczne wcięcia (D024): Enter zachowuje wcięcie; w językach z klamrami
// „{” wcina, „}” cofa; Tab wstawia spacje albo tabulator zgodnie ze stylem pliku.
// Pisanie nigdy nie otwiera podpowiedzi.

#include "editor/EditorWidget.h"

#include <QtTest>

using namespace cynamoncode;

#define VERIFY_NO_SUGGESTIONS(editor)                                                    \
    do {                                                                                 \
        QVERIFY((editor).autoCompletionSource() == QsciScintilla::AcsNone);              \
        QVERIFY(!(editor).isListActive());                                               \
        QVERIFY(!(editor).isCallTipActive());                                            \
    } while (false)

namespace {

// keyClicks przyjmuje tylko drukowalne ASCII — Enter i Tab wysyłamy osobno.
void typeText(EditorWidget &editor, const char *text)
{
    QTest::keyClicks(&editor, QLatin1String(text));
}

void pressEnter(EditorWidget &editor)
{
    QTest::keyClick(&editor, Qt::Key_Return);
}

} // namespace

class TestAutoIndent : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void plainTextKeepsIndentation();
    void cppBracesIndentAndUnindent();
    void javaScriptAndCssUseBraces_data();
    void javaScriptAndCssUseBraces();
    void phpBracesIndent();
    void pythonOnlyKeepsIndentation();
    void htmlTextKeepsIndentation();
    void htmlOpeningTagIndentsClosingUnindents();
    void htmlNonBlockTagsKeepIndentation_data();
    void htmlNonBlockTagsKeepIndentation();
    void htmlEnterBetweenTagsKeepsClosingTagLevel();
    void htmlTagIndentIsOneUndoStep();
    void xmlTagsIndent();
    void phpHtmlPartUsesTagsCodeUsesBraces();
    void scriptContentIsNotATag();
    void tabsWhenDocumentUsesTabs();
    void tabKeyInsertsFourSpaces();

private:
    EditorWidget *m_editor = nullptr;
};

void TestAutoIndent::init()
{
    m_editor = new EditorWidget;
    m_editor->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_editor));
    m_editor->setFocus();
}

void TestAutoIndent::cleanup()
{
    delete m_editor;
    m_editor = nullptr;
}

void TestAutoIndent::plainTextKeepsIndentation()
{
    QVERIFY(m_editor->autoIndent());
    typeText(*m_editor, "  lista:");
    pressEnter(*m_editor);
    typeText(*m_editor, "x {");
    pressEnter(*m_editor);
    // Zwykły tekst: klamra niczego nie zmienia.
    QCOMPARE(m_editor->text(), QStringLiteral("  lista:\n  x {\n  "));
    VERIFY_NO_SUGGESTIONS(*m_editor);
}

void TestAutoIndent::cppBracesIndentAndUnindent()
{
    m_editor->setLanguage(LanguageId::Cpp);
    typeText(*m_editor, "int f() {");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("int f() {\n    "));

    typeText(*m_editor, "return 0;");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("int f() {\n    return 0;\n    "));

    typeText(*m_editor, "}");
    QCOMPARE(m_editor->text(), QStringLiteral("int f() {\n    return 0;\n}"));
    VERIFY_NO_SUGGESTIONS(*m_editor);
}

void TestAutoIndent::javaScriptAndCssUseBraces_data()
{
    QTest::addColumn<int>("language");
    QTest::addColumn<QString>("opening");
    QTest::newRow("java") << int(LanguageId::Java) << QStringLiteral("class A {");
    QTest::newRow("javascript") << int(LanguageId::JavaScript) << QStringLiteral("if (x) {");
    QTest::newRow("css") << int(LanguageId::Css) << QStringLiteral("body {");
}

void TestAutoIndent::javaScriptAndCssUseBraces()
{
    QFETCH(int, language);
    QFETCH(QString, opening);
    m_editor->setLanguage(static_cast<LanguageId>(language));
    QTest::keyClicks(m_editor, opening);
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), opening + QStringLiteral("\n    "));
    VERIFY_NO_SUGGESTIONS(*m_editor);
}

void TestAutoIndent::phpBracesIndent()
{
    m_editor->setLanguage(LanguageId::Php);
    QVERIFY(m_editor->loadText(QStringLiteral("<?php\n")));
    m_editor->setCursorPosition(1, 0);
    typeText(*m_editor, "if ($x) {");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<?php\nif ($x) {\n    "));
    VERIFY_NO_SUGGESTIONS(*m_editor);
}

void TestAutoIndent::pythonOnlyKeepsIndentation()
{
    m_editor->setLanguage(LanguageId::Python);
    typeText(*m_editor, "def f():");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("def f():\n"));

    typeText(*m_editor, "    x = 1");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("def f():\n    x = 1\n    "));
    VERIFY_NO_SUGGESTIONS(*m_editor);
}

void TestAutoIndent::htmlTextKeepsIndentation()
{
    m_editor->setLanguage(LanguageId::Html);
    typeText(*m_editor, "  tekst");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("  tekst\n  "));
}

void TestAutoIndent::htmlOpeningTagIndentsClosingUnindents()
{
    m_editor->setLanguage(LanguageId::Html);
    typeText(*m_editor, "<ul class=\"menu\">");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<ul class=\"menu\">\n    "));

    typeText(*m_editor, "<li>a</li>");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<ul class=\"menu\">\n    <li>a</li>\n    "));

    typeText(*m_editor, "</ul>");
    QCOMPARE(m_editor->text(), QStringLiteral("<ul class=\"menu\">\n    <li>a</li>\n</ul>"));
    VERIFY_NO_SUGGESTIONS(*m_editor);
}

void TestAutoIndent::htmlNonBlockTagsKeepIndentation_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("element pusty") << QStringLiteral("<br>");
    QTest::newRow("img z atrybutami") << QStringLiteral("<img src=\"a/b.png\" alt=\"x\">");
    QTest::newRow("samozamykający") << QStringLiteral("<x-ikona/>");
    QTest::newRow("zamknięty w linii") << QStringLiteral("<p>tekst</p>");
    QTest::newRow("komentarz") << QStringLiteral("<!-- <div> -->");
    QTest::newRow("doctype") << QStringLiteral("<!DOCTYPE html>");
    // „  </div>” celowo nie jest tu sprawdzane: wpisanie „</” cofa wcięcie
    // (htmlOpeningTagIndentsClosingUnindents).
}

void TestAutoIndent::htmlNonBlockTagsKeepIndentation()
{
    QFETCH(QString, line);
    m_editor->setLanguage(LanguageId::Html);
    QTest::keyClicks(m_editor, QStringLiteral("  ") + line);
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("  ") + line + QStringLiteral("\n  "));
}

void TestAutoIndent::htmlEnterBetweenTagsKeepsClosingTagLevel()
{
    m_editor->setLanguage(LanguageId::Html);
    QVERIFY(m_editor->loadText(QStringLiteral("<div></div>")));
    m_editor->setCursorPosition(0, 5);
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<div>\n</div>"));
}

void TestAutoIndent::htmlTagIndentIsOneUndoStep()
{
    m_editor->setLanguage(LanguageId::Html);
    typeText(*m_editor, "<div>");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<div>\n    "));
    m_editor->undo();
    QCOMPARE(m_editor->text(), QStringLiteral("<div>"));
}

void TestAutoIndent::xmlTagsIndent()
{
    m_editor->setLanguage(LanguageId::Xml);
    typeText(*m_editor, "<root>");
    pressEnter(*m_editor);
    typeText(*m_editor, "<br/>");
    pressEnter(*m_editor);
    typeText(*m_editor, "<br>"); // w XML to zwykły element otwierający
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<root>\n    <br/>\n    <br>\n        "));
}

void TestAutoIndent::phpHtmlPartUsesTagsCodeUsesBraces()
{
    m_editor->setLanguage(LanguageId::Php);
    QVERIFY(m_editor->loadText(QStringLiteral("<?php $a = 1; ?>\n")));
    m_editor->setCursorPosition(1, 0);
    typeText(*m_editor, "<div>");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<?php $a = 1; ?>\n<div>\n    "));

    // W kodzie PHP „<” to operator — tylko klamra wcina (jeden poziom, nie dwa).
    m_editor->selectAll();
    m_editor->replaceSelectedText(QStringLiteral("<?php\n"));
    m_editor->setCursorPosition(1, 0);
    typeText(*m_editor, "if ($a < 3) {");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<?php\nif ($a < 3) {\n    "));
}

void TestAutoIndent::scriptContentIsNotATag()
{
    m_editor->setLanguage(LanguageId::Html);
    QVERIFY(m_editor->loadText(QStringLiteral("<script>\n")));
    m_editor->setCursorPosition(1, 0);
    typeText(*m_editor, "if (a <b> c) x();");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("<script>\nif (a <b> c) x();\n"));
}

void TestAutoIndent::tabsWhenDocumentUsesTabs()
{
    m_editor->setLanguage(LanguageId::Cpp);
    m_editor->setIndentationsUseTabs(true);
    typeText(*m_editor, "if (x) {");
    pressEnter(*m_editor);
    QCOMPARE(m_editor->text(), QStringLiteral("if (x) {\n\t"));

    // Zmiana lexera nie przywraca spacji.
    m_editor->setLanguage(LanguageId::Python);
    QVERIFY(m_editor->indentationsUseTabs());
}

void TestAutoIndent::tabKeyInsertsFourSpaces()
{
    QTest::keyClick(m_editor, Qt::Key_Tab);
    typeText(*m_editor, "x");
    QCOMPARE(m_editor->text(), QStringLiteral("    x"));
}

QTEST_MAIN(TestAutoIndent)
#include "tst_auto_indent.moc"
