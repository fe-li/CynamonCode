#pragma once

#include "domain/SearchOptions.h"

#include <QList>
#include <QString>

#include <optional>

namespace cynamoncode::SearchService {

// Pusty napis = wzorzec poprawny. Błąd opisuje wyłącznie wzorzec wyszukiwania
// wpisany przez użytkownika (nie dotyczy treści dokumentu).
QString patternError(const SearchOptions &options);

// Następne (forward) lub poprzednie dopasowanie względem `from`.
// forward: pierwsze dopasowanie zaczynające się od `from` (puste dopasowanie
// dokładnie w `from` jest pomijane, żeby nie stać w miejscu).
// backward: ostatnie dopasowanie zaczynające się przed `from`.
// Z `wrap` szukanie przechodzi przez początek/koniec tekstu.
std::optional<SearchMatch> find(const QString &text, const SearchOptions &options, qsizetype from,
                                bool forward);

// Dopasowanie zaczynające się dokładnie w `start` (do „Zamień” na zaznaczeniu).
std::optional<SearchMatch> matchAt(const QString &text, const SearchOptions &options,
                                   qsizetype start);

// Tekst zastępujący dla dopasowania w `start`; w trybie regex rozwija \0–\9, \n, \t, \\.
QString replacementFor(const QString &text, const SearchOptions &options, qsizetype start,
                       const QString &replacement);

// Dopasowania w kolejności rosnącej (np. „Znajdź we wszystkich”); co najwyżej
// `limit` pierwszych (ujemny = bez limitu).
QList<SearchMatch> findAll(const QString &text, const SearchOptions &options,
                           qsizetype limit = -1);

// Sama liczba dopasowań — bez zbierania listy (np. dla wzorców pustej długości).
qsizetype countMatches(const QString &text, const SearchOptions &options);

// Wszystkie zamiany w kolejności rosnącej, bez nakładania się.
QList<Replacement> replaceAll(const QString &text, const SearchOptions &options,
                              const QString &replacement);

} // namespace cynamoncode::SearchService
