// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

// SearchService i TextPositions: wyszukiwanie zwykłe, bez wielkości liter,
// całe wyrazy, regex z grupami, zawijanie, puste dopasowania; przeliczanie
// indeksów QString ↔ bajty UTF-8 (D028, T17).

#include "services/SearchService.h"
#include "services/TextPositions.h"

#include <QtTest>

using namespace cynamoncode;

namespace {

SearchOptions literal(const QString &pattern)
{
    SearchOptions options;
    options.pattern = pattern;
    return options;
}

SearchOptions regex(const QString &pattern)
{
    SearchOptions options = literal(pattern);
    options.regex = true;
    return options;
}

QString applyAll(const QString &text, const QList<Replacement> &replacements)
{
    QString result = text;
    for (qsizetype i = replacements.size() - 1; i >= 0; --i)
        result.replace(replacements.at(i).start, replacements.at(i).length, replacements.at(i).text);
    return result;
}

} // namespace

class TestSearch : public QObject
{
    Q_OBJECT

private slots:
    void findsForwardAndWraps();
    void findsBackwardAndWraps();
    void caseInsensitivePolish();
    void caseSensitive();
    void wholeWordsOnly();
    void literalTreatsRegexCharactersLiterally();
    void regexGroupsInReplacement();
    void regexAnchorsWorkWithCrLf();
    void regexEscapesInReplacement();
    void emptyMatchesDoNotLoop();
    void invalidRegexIsReported();
    void emptyPatternFindsNothing();
    void matchAtRequiresExactStart();
    void utf8OffsetsRoundTrip();
};

void TestSearch::findsForwardAndWraps()
{
    const QString text = QStringLiteral("jeden dwa jeden");
    SearchOptions options = literal(QStringLiteral("jeden"));
    QCOMPARE(SearchService::find(text, options, 0, true)->start, 0);
    QCOMPARE(SearchService::find(text, options, 5, true)->start, 10);
    QCOMPARE(SearchService::find(text, options, 15, true)->start, 0); // zawinięcie
    options.wrap = false;
    QVERIFY(!SearchService::find(text, options, 11, true));
}

void TestSearch::findsBackwardAndWraps()
{
    const QString text = QStringLiteral("jeden dwa jeden");
    SearchOptions options = literal(QStringLiteral("jeden"));
    QCOMPARE(SearchService::find(text, options, 10, false)->start, 0);
    QCOMPARE(SearchService::find(text, options, 0, false)->start, 10); // zawinięcie
    options.wrap = false;
    QVERIFY(!SearchService::find(text, options, 0, false));
}

void TestSearch::caseInsensitivePolish()
{
    const QString text = QStringLiteral("Mały żółw i ŻÓŁW");
    const SearchOptions options = literal(QStringLiteral("żółw"));
    const auto first = SearchService::find(text, options, 0, true);
    QCOMPARE(first->start, 5);
    QCOMPARE(SearchService::find(text, options, first->start + first->length, true)->start, 12);
    QCOMPARE(SearchService::replaceAll(text, options, QStringLiteral("kot")).size(), 2);
}

void TestSearch::caseSensitive()
{
    SearchOptions options = literal(QStringLiteral("żółw"));
    options.caseSensitive = true;
    QCOMPARE(SearchService::replaceAll(QStringLiteral("Żółw żółw ŻÓŁW"), options, QString()).size(),
             1);
}

void TestSearch::wholeWordsOnly()
{
    SearchOptions options = literal(QStringLiteral("kot"));
    options.wholeWords = true;
    const QString text = QStringLiteral("kot kotek, kot. pokot kot");
    const QList<Replacement> found = SearchService::replaceAll(text, options, QStringLiteral("pies"));
    QCOMPARE(found.size(), 3);
    QCOMPARE(applyAll(text, found), QStringLiteral("pies kotek, pies. pokot pies"));
}

void TestSearch::literalTreatsRegexCharactersLiterally()
{
    const QString text = QStringLiteral("a.b axb (a.b)");
    const QList<Replacement> found =
        SearchService::replaceAll(text, literal(QStringLiteral("a.b")), QStringLiteral("\\1"));
    QCOMPARE(found.size(), 2);
    QCOMPARE(applyAll(text, found), QStringLiteral("\\1 axb (\\1)"));
}

void TestSearch::regexGroupsInReplacement()
{
    const QString text = QStringLiteral("a1 b2 c3");
    const QList<Replacement> found = SearchService::replaceAll(
        text, regex(QStringLiteral("([a-z])(\\d)")), QStringLiteral("\\2\\1"));
    QCOMPARE(applyAll(text, found), QStringLiteral("1a 2b 3c"));
}

void TestSearch::regexAnchorsWorkWithCrLf()
{
    const QString text = QStringLiteral("jeden\r\ndwa\r\ntrzy");
    const QList<Replacement> found =
        SearchService::replaceAll(text, regex(QStringLiteral("^(\\w+)$")), QStringLiteral("[\\1]"));
    QCOMPARE(applyAll(text, found), QStringLiteral("[jeden]\r\n[dwa]\r\n[trzy]"));
}

void TestSearch::regexEscapesInReplacement()
{
    const QString text = QStringLiteral("a,b");
    const QList<Replacement> found =
        SearchService::replaceAll(text, regex(QStringLiteral(",")), QStringLiteral("\\n\\t\\\\"));
    QCOMPARE(applyAll(text, found), QStringLiteral("a\n\t\\b"));
}

void TestSearch::emptyMatchesDoNotLoop()
{
    const QString text = QStringLiteral("a\nb\nc");
    const SearchOptions options = regex(QStringLiteral("^"));
    // Puste dopasowanie w miejscu kursora jest pomijane — szukanie idzie dalej.
    QCOMPARE(SearchService::find(text, options, 0, true)->start, 2);
    QCOMPARE(SearchService::find(text, options, 2, true)->start, 4);
    const QList<Replacement> found = SearchService::replaceAll(text, options, QStringLiteral("> "));
    QCOMPARE(applyAll(text, found), QStringLiteral("> a\n> b\n> c"));

    // Jedyne dopasowanie to puste w miejscu kursora — nadal zostaje znalezione.
    const auto only = SearchService::find(QStringLiteral("abc"), options, 0, true);
    QVERIFY(only);
    QCOMPARE(only->start, 0);
    QCOMPARE(only->length, 0);
}

void TestSearch::invalidRegexIsReported()
{
    const SearchOptions options = regex(QStringLiteral("(niezamknięty"));
    QVERIFY(!SearchService::patternError(options).isEmpty());
    QVERIFY(!SearchService::find(QStringLiteral("(niezamknięty"), options, 0, true));
    QVERIFY(SearchService::replaceAll(QStringLiteral("x"), options, QString()).isEmpty());
    QVERIFY(SearchService::patternError(literal(QStringLiteral("(niezamknięty"))).isEmpty());
}

void TestSearch::emptyPatternFindsNothing()
{
    QVERIFY(!SearchService::find(QStringLiteral("abc"), literal(QString()), 0, true));
    QVERIFY(SearchService::replaceAll(QStringLiteral("abc"), literal(QString()), QStringLiteral("x"))
                .isEmpty());
}

void TestSearch::matchAtRequiresExactStart()
{
    const QString text = QStringLiteral("xx kot");
    QVERIFY(!SearchService::matchAt(text, literal(QStringLiteral("kot")), 0));
    QCOMPARE(SearchService::matchAt(text, literal(QStringLiteral("kot")), 3)->length, 3);
}

void TestSearch::utf8OffsetsRoundTrip()
{
    // a(1 bajt) ż(2) 😀(4, dwie jednostki UTF-16) b(1)
    const QString text = QStringLiteral("aż😀b");
    QCOMPARE(text.size(), 5);
    const QList<qsizetype> bytes = TextPositions::toUtf8Offsets(text, {0, 1, 2, 4, 5});
    QCOMPARE(bytes, (QList<qsizetype>{0, 1, 3, 7, 8}));
    QCOMPARE(TextPositions::toUtf16Index(text, 0), qsizetype(0));
    QCOMPARE(TextPositions::toUtf16Index(text, 3), qsizetype(2));
    QCOMPARE(TextPositions::toUtf16Index(text, 7), qsizetype(4));
    QCOMPARE(TextPositions::toUtf16Index(text, 8), qsizetype(5));
    QCOMPARE(TextPositions::toUtf16Index(text, 5), qsizetype(4)); // środek emoji → za nim
    QCOMPARE(text.toUtf8().size(), qsizetype(8));
}

QTEST_GUILESS_MAIN(TestSearch)
#include "tst_search.moc"
