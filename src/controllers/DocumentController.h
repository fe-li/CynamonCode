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
    File,                  // szczegóły w fileError / systemMessage
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
    DocumentId createNew();
    OpenOutcome open(const QString &path);
    SaveOutcome save(DocumentId id, const QString &text);
    SaveOutcome saveAs(DocumentId id, const QString &path, const QString &text);
    void close(DocumentId id);

    const DocumentState *document(DocumentId id) const { return m_documents.find(id); }
    qsizetype documentCount() const { return m_documents.count(); }

private:
    SaveOutcome writeDocument(DocumentState &document, const QString &path, const QString &text);

    DocumentManager m_documents;
};

} // namespace cynamoncode
