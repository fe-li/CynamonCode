// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "domain/DocumentState.h"
#include "services/DocumentManager.h"
#include "services/FileService.h"

#include <QString>

namespace cynamoncode {

enum class OpenStatus {
    Opened,
    AlreadyOpen, // ta sama ścieżka kanoniczna jest już otwarta — id istniejącego dokumentu
    Failed,
};

enum class OpenFailure {
    None,
    File,        // szczegóły w fileError / systemMessage
    Binary,      // bajty NUL
    InvalidUtf8,
};

struct OpenOutcome
{
    OpenStatus status = OpenStatus::Failed;
    DocumentId id = InvalidDocumentId;
    QString text; // tylko dla Opened
    OpenFailure failure = OpenFailure::None;
    FileError fileError = FileError::None;
    QString systemMessage;
};

enum class SaveFailure {
    None,
    UnknownDocument,
    NoPath,                // dokument bez pliku — potrzebne „Zapisz jako”
    OpenInAnotherDocument, // docelowy plik jest otwarty w innej karcie
    DiskChanged,           // plik na dysku zmienił się od wczytania/zapisu (D027)
    File,                  // szczegóły w fileError / systemMessage
};

enum class DiskStatus {
    Unchanged,
    Changed, // inna treść niż przy ostatnim wczytaniu/zapisie
    Missing, // plik usunięty lub przeniesiony
};

struct SaveOutcome
{
    SaveFailure failure = SaveFailure::None;
    FileError fileError = FileError::None;
    QString systemMessage;

    bool ok() const { return failure == SaveFailure::None; }
};

// Przypadki użycia na dokumentach: nowy, otwórz, zapisz, zapisz jako, zamknij.
// Bez dialogów i widżetów — o pytaniach do użytkownika decyduje UI.
class DocumentController
{
public:
    // Powyżej tego rozmiaru UI pyta przed otwarciem (D039, SPECYFIKACJA §3.4).
    static constexpr qint64 LargeFileWarningBytes = 20LL * 1024 * 1024;

    DocumentId createNew();
    OpenOutcome open(const QString &path);
    // Bez `overwriteDiskChanges` zapis odmawia (DiskChanged), gdy plik na dysku
    // zmienił się od ostatniego wczytania lub zapisu (D027).
    SaveOutcome save(DocumentId id, const QString &text, bool overwriteDiskChanges = false);
    SaveOutcome saveAs(DocumentId id, const QString &path, const QString &text);
    void close(DocumentId id);

    // Porównanie pliku na dysku z ostatnim stanem (D030); sam `touch` = Unchanged.
    // `trustMetadata` pozwala uznać zgodny rozmiar i mtime za brak zmian (tani
    // test np. przy zamykaniu); zapis i zdarzenia obserwatora zawsze liczą SHA-256.
    DiskStatus checkDisk(DocumentId id, bool trustMetadata = true);

    // Użytkownik wybrał „Zostaw” dla bieżącej wersji na dysku: kolejne zdarzenia
    // z tą samą treścią nie pytają ponownie (zapis nadal zgłosi konflikt, D027).
    void declineDiskVersion(DocumentId id);
    bool isDiskVersionDeclined(DocumentId id) const;

    // Ponowne wczytanie w dwóch krokach: prepareReload niczego nie zmienia,
    // commitReload zapisuje nowy stan dopiero po udanym wczytaniu do edytora.
    struct ReloadData
    {
        OpenOutcome outcome;
        DocumentState state; // hasBom, lineEnding, indentWithTabs, disk
    };
    ReloadData prepareReload(DocumentId id) const;
    void commitReload(DocumentId id, const ReloadData &data);
    // prepareReload + commitReload (bez edytora).
    OpenOutcome reload(DocumentId id);

    // Ręczny wybór języka dla dokumentu (D023).
    void setLanguage(DocumentId id, LanguageId language);
    // Po świadomym ujednoliceniu końców linii przez użytkownika (D020).
    void setLineEnding(DocumentId id, LineEnding lineEnding);

    const DocumentState *document(DocumentId id) const { return m_documents.find(id); }
    // Dokument otwarty pod tą ścieżką (porównanie ścieżek kanonicznych).
    std::optional<DocumentId> findOpen(const QString &path) const;
    qsizetype documentCount() const { return m_documents.count(); }

private:
    SaveOutcome writeDocument(DocumentState &document, const QString &path, const QString &text);

    DocumentManager m_documents;
};

} // namespace cynamoncode
