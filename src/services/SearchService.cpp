// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/SearchService.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>

namespace cynamoncode::SearchService {

namespace {

QRegularExpression buildExpression(const SearchOptions &options)
{
    QString pattern =
        options.regex ? options.pattern : QRegularExpression::escape(options.pattern);
    if (options.wholeWords)
        pattern = QStringLiteral("(?<!\\w)(?:") + pattern + QStringLiteral(")(?!\\w)");
    if (options.regex) {
        // ^ i $ na granicach linii także dla CRLF i samotnego CR.
        pattern.prepend(QStringLiteral("(*ANYCRLF)"));
    }

    QRegularExpression::PatternOptions flags = QRegularExpression::UseUnicodePropertiesOption;
    if (options.regex)
        flags |= QRegularExpression::MultilineOption;
    if (!options.caseSensitive)
        flags |= QRegularExpression::CaseInsensitiveOption;
    return QRegularExpression(pattern, flags);
}

std::optional<SearchMatch> toMatch(const QRegularExpressionMatch &match)
{
    if (!match.hasMatch())
        return std::nullopt;
    return SearchMatch{match.capturedStart(), match.capturedLength()};
}

QString expand(const QRegularExpressionMatch &match, const QString &replacement)
{
    QString out;
    out.reserve(replacement.size());
    for (qsizetype i = 0; i < replacement.size(); ++i) {
        const QChar c = replacement.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= replacement.size()) {
            out += c;
            continue;
        }
        const QChar next = replacement.at(++i);
        // Tylko cyfry ASCII (QChar::isDigit przyjmuje też cyfry innych pism).
        if (next >= QLatin1Char('0') && next <= QLatin1Char('9'))
            out += match.captured(next.unicode() - u'0');
        else if (next == QLatin1Char('n'))
            out += QLatin1Char('\n');
        else if (next == QLatin1Char('t'))
            out += QLatin1Char('\t');
        else if (next == QLatin1Char('\\'))
            out += QLatin1Char('\\');
        else {
            out += c;
            out += next;
        }
    }
    return out;
}

} // namespace

QString patternError(const SearchOptions &options)
{
    if (!options.regex || options.pattern.isEmpty())
        return QString();
    const QRegularExpression expression = buildExpression(options);
    if (expression.isValid())
        return QString();
    return QCoreApplication::translate("SearchService", "Niepoprawne wyrażenie regularne: %1")
        .arg(expression.errorString());
}

std::optional<SearchMatch> find(const QString &text, const SearchOptions &options, qsizetype from,
                                bool forward)
{
    if (options.pattern.isEmpty())
        return std::nullopt;
    const QRegularExpression expression = buildExpression(options);
    if (!expression.isValid())
        return std::nullopt;
    from = std::clamp<qsizetype>(from, 0, text.size());

    if (forward) {
        std::optional<SearchMatch> skippedEmpty;
        const auto firstFrom = [&](qsizetype offset) -> std::optional<SearchMatch> {
            QRegularExpressionMatchIterator it = expression.globalMatch(text, offset);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                if (match.capturedLength() == 0 && match.capturedStart() == from) {
                    skippedEmpty = toMatch(match); // puste dopasowanie w miejscu kursora — dalej
                    continue;
                }
                return toMatch(match);
            }
            return std::nullopt;
        };
        if (auto match = firstFrom(from))
            return match;
        if (options.wrap && from > 0) {
            auto wrapped = firstFrom(0);
            if (wrapped && wrapped->start < from)
                return wrapped;
        }
        // Jedyne dopasowanie to puste w miejscu kursora (np. „^” w jednej linii).
        return skippedEmpty;
    }

    std::optional<SearchMatch> before;
    std::optional<SearchMatch> last;
    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext()) {
        const std::optional<SearchMatch> match = toMatch(it.next());
        if (!match)
            continue;
        if (match->start < from)
            before = match;
        last = match;
    }
    if (before)
        return before;
    if (options.wrap)
        return last;
    return std::nullopt;
}

std::optional<SearchMatch> matchAt(const QString &text, const SearchOptions &options,
                                   qsizetype start)
{
    if (options.pattern.isEmpty())
        return std::nullopt;
    const QRegularExpression expression = buildExpression(options);
    if (!expression.isValid())
        return std::nullopt;
    const QRegularExpressionMatch match = expression.match(
        text, start, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
    return toMatch(match);
}

QString replacementFor(const QString &text, const SearchOptions &options, qsizetype start,
                       const QString &replacement)
{
    if (!options.regex)
        return replacement;
    const QRegularExpression expression = buildExpression(options);
    const QRegularExpressionMatch match = expression.match(
        text, start, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
    return match.hasMatch() ? expand(match, replacement) : replacement;
}

QList<SearchMatch> findAll(const QString &text, const SearchOptions &options, qsizetype limit)
{
    QList<SearchMatch> result;
    if (options.pattern.isEmpty() || limit == 0)
        return result;
    const QRegularExpression expression = buildExpression(options);
    if (!expression.isValid())
        return result;
    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext() && (limit < 0 || result.size() < limit)) {
        const QRegularExpressionMatch match = it.next();
        result.append({match.capturedStart(), match.capturedLength()});
    }
    return result;
}

qsizetype countMatches(const QString &text, const SearchOptions &options)
{
    if (options.pattern.isEmpty())
        return 0;
    const QRegularExpression expression = buildExpression(options);
    if (!expression.isValid())
        return 0;
    qsizetype count = 0;
    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

QList<Replacement> replaceAll(const QString &text, const SearchOptions &options,
                              const QString &replacement)
{
    QList<Replacement> result;
    if (options.pattern.isEmpty())
        return result;
    const QRegularExpression expression = buildExpression(options);
    if (!expression.isValid())
        return result;

    QRegularExpressionMatchIterator it = expression.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result.append({match.capturedStart(), match.capturedLength(),
                       options.regex ? expand(match, replacement) : replacement});
    }
    return result;
}

} // namespace cynamoncode::SearchService
