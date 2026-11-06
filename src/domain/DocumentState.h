#pragma once

#include "domain/LanguageId.h"
#include "domain/LineEnding.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace cynamoncode {

using DocumentId = quint64;
inline constexpr DocumentId InvalidDocumentId = 0;

// Stan pliku na dysku po ostatnim wczytaniu lub zapisie (D030).
struct DiskSnapshot
{
    bool known = false;
    qint64 size = -1;
    QDateTime modified;
    QByteArray sha256;
};

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
    DiskSnapshot disk;
    bool missingOnDisk = false; // plik usunięty/przeniesiony — zamknięcie pyta o zapis (D026)
    QByteArray lastSeenDiskSha256;  // skrót ostatnio sprawdzonej, zmienionej wersji na dysku
    QByteArray declinedDiskSha256;  // wersja, przy której użytkownik wybrał „Zostaw”

    bool isUntitled() const { return filePath.isEmpty(); }
};

} // namespace cynamoncode
