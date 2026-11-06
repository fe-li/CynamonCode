// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// Regresja bezwzględnego zakazu z CLAUDE.md: żadnego autouzupełniania ani
// call tips — po utworzeniu edytora, podczas pisania i po zmianie lexera
// (TESTY.md T08, T09; D013).

#include "editor/EditorWidget.h"

#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerpython.h>

#include <QtTest>

using cynamoncode::EditorWidget;

#define VERIFY_NO_SUGGESTIONS(editor)                                                    \
    do {                                                                                 \
        QVERIFY((editor).autoCompletionSource() == QsciScintilla::AcsNone);              \
        QVERIFY((editor).autoCompletionThreshold() < 0);                                 \
        QVERIFY((editor).callTipsStyle() == QsciScintilla::CallTipsNone);                \
        QVERIFY(!(editor).isListActive());                                               \
        QVERIFY(!(editor).isCallTipActive());                                            \
    } while (false)

class TestEditorNoSuggestions : public QObject
{
    Q_OBJECT

private slots:
    void newEditorHasSuggestionsDisabled();
    void typingNeverOpensSuggestions();
    void lexerChangeKeepsSuggestionsDisabled();
    void loadTextIsUnmodifiedWithoutUndo();
};

void TestEditorNoSuggestions::newEditorHasSuggestionsDisabled()
{
    EditorWidget editor;
    VERIFY_NO_SUGGESTIONS(editor);
}

void TestEditorNoSuggestions::typingNeverOpensSuggestions()
{
    EditorWidget editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setFocus();

    // Słowa powtarzają się, żeby ewentualne AcsDocument miało z czego podpowiadać.
    QTest::keyClicks(&editor, QStringLiteral("include include incl"));
    VERIFY_NO_SUGGESTIONS(editor);

    // keyClicks przyjmuje tylko drukowalne ASCII — Enter osobno.
    QTest::keyClick(&editor, Qt::Key_Return);
    QTest::keyClicks(&editor, QStringLiteral("std::vector v; v."));
    VERIFY_NO_SUGGESTIONS(editor);

    QTest::keyClicks(&editor, QStringLiteral("push_back("));
    VERIFY_NO_SUGGESTIONS(editor);

    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    VERIFY_NO_SUGGESTIONS(editor);

    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier | Qt::ShiftModifier);
    VERIFY_NO_SUGGESTIONS(editor);
}

void TestEditorNoSuggestions::lexerChangeKeepsSuggestionsDisabled()
{
    EditorWidget editor;
    const QString text = QStringLiteral("int main() {\n    return 0;\n}\n");
    editor.setText(text);
    editor.setCursorPosition(1, 4);

    editor.setLexer(new QsciLexerCPP(&editor));
    VERIFY_NO_SUGGESTIONS(editor);

    editor.setLexer(new QsciLexerPython(&editor));
    VERIFY_NO_SUGGESTIONS(editor);

    editor.setLexer(nullptr);
    VERIFY_NO_SUGGESTIONS(editor);

    QCOMPARE(editor.text(), text);
    int line = -1;
    int index = -1;
    editor.getCursorPosition(&line, &index);
    QCOMPARE(line, 1);
    QCOMPARE(index, 4);
}

void TestEditorNoSuggestions::loadTextIsUnmodifiedWithoutUndo()
{
    EditorWidget editor;
    const QString text = QStringLiteral("Zażółć gęślą jaźń 😀\r\ndruga linia\r\n");
    QVERIFY(editor.loadText(text));
    QCOMPARE(editor.text(), text);
    QVERIFY(!editor.isModified());
    QVERIFY(!editor.isUndoAvailable());
    VERIFY_NO_SUGGESTIONS(editor);
}

QTEST_MAIN(TestEditorNoSuggestions)
#include "tst_editor_no_suggestions.moc"
