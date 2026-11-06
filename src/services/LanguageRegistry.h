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

// Jednoznaczny początek dokumentu: <!DOCTYPE html>, <html, <?xml, <?php (D025).
LanguageId fromFirstLine(const QString &firstLine);

// Proponowane rozszerzenie (bez kropki) przy zapisie nowego dokumentu; puste dla zwykłego tekstu.
QString defaultExtension(LanguageId language);

// Kolejność z D023/D025 (bez ręcznego wyboru, który przechowuje dokument):
// rozszerzenie / nazwa pliku → shebang → (tylko plik bez rozszerzenia) początek
// treści → zwykły tekst.
LanguageId detect(const QString &filePath, const QString &firstLine);

} // namespace cynamoncode::LanguageRegistry
