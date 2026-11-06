#pragma once

#include "domain/LanguageId.h"

#include <QList>
#include <QString>

namespace cynamoncode::LanguageRegistry {

// Wszystkie obsługiwane języki w kolejności menu „Język”.
const QList<LanguageId> &all();

// Stały, nietłumaczony klucz (np. "cpp") — do nazw obiektów i przyszłych ustawień.
QString key(LanguageId language);

// Nazwa do wyświetlenia w interfejsie (tłumaczona).
QString displayName(LanguageId language);

// Rozszerzenie lub znana nazwa pliku; Plain, jeśli nieznane.
LanguageId fromFileName(const QString &filePath);

// Pierwsza linia z „#!”; Plain, jeśli nie rozpoznano.
LanguageId fromShebang(const QString &firstLine);

// Kolejność z D023 (bez ręcznego wyboru, który przechowuje dokument):
// rozszerzenie / nazwa pliku → shebang → zwykły tekst.
LanguageId detect(const QString &filePath, const QString &firstLine);

} // namespace cynamoncode::LanguageRegistry
