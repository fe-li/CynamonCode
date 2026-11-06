#pragma once

#include "domain/LanguageId.h"
#include "domain/LineEnding.h"

#include <QString>
#include <QtGlobal>

namespace cynamoncode {

using DocumentId = quint64;
inline constexpr DocumentId InvalidDocumentId = 0;

// Metadane otwartego dokumentu. Treść i flaga modyfikacji żyją w buforze
// QScintilla (EditorWidget) — tu celowo nie ma kopii tekstu (D014).
struct DocumentState
{
    DocumentId id = InvalidDocumentId;
    QString filePath;       // ścieżka bezwzględna podana przez użytkownika; pusta dla nowego dokumentu
    QString canonicalPath;  // do wykrywania duplikatów kart; pusta, dopóki plik nie istnieje
    int untitledNumber = 0; // numer „Nowy N” dla dokumentu bez pliku
    bool hasBom = false;
    LineEnding lineEnding = LineEnding::None;
    LanguageId language = LanguageId::Plain;
    bool languageChosenManually = false; // ręczny wybór ma pierwszeństwo (D023)
    bool indentWithTabs = false;         // wykryte z pliku; nowe dokumenty — spacje (D024)

    bool isUntitled() const { return filePath.isEmpty(); }
};

} // namespace cynamoncode
