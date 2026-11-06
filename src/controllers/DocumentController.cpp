#include "controllers/DocumentController.h"

#include "services/EncodingService.h"
#include "services/Indentation.h"
#include "services/LanguageRegistry.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <utility>

namespace cynamoncode {

namespace {

// Pierwsza linia (do wykrywania shebangu); wystarczy krótki początek tekstu.
QString firstLine(const QString &text)
{
    QString line = text.left(256).section(QLatin1Char('\n'), 0, 0);
    if (line.endsWith(QLatin1Char('\r')))
        line.chop(1);
    return line;
}

QByteArray sha256(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

// Rozmiar zawsze z faktycznych bajtów. Przy odczycie czas modyfikacji pobieramy
// PRZED odczytem (`modifiedBeforeRead`): zmiana w trakcie odczytu daje wtedy
// niezgodny czas i wymusza porównanie skrótu, zamiast utrwalić niespójną migawkę.
DiskSnapshot snapshotOf(const QByteArray &bytes, const QDateTime &modified)
{
    DiskSnapshot snapshot;
    snapshot.known = true;
    snapshot.size = bytes.size();
    snapshot.modified = modified;
    snapshot.sha256 = sha256(bytes);
    return snapshot;
}

QDateTime modifiedTime(const QString &path)
{
    return QFileInfo(path).lastModified();
}

} // namespace

DocumentId DocumentController::createNew()
{
    return m_documents.createUntitled();
}

OpenOutcome DocumentController::open(const QString &path)
{
    OpenOutcome outcome;
    const QFileInfo info(path);

    const QString canonical = info.canonicalFilePath();
    if (const auto existing = m_documents.findByCanonicalPath(canonical)) {
        outcome.status = OpenStatus::AlreadyOpen;
        outcome.id = *existing;
        return outcome;
    }

    const QDateTime modifiedBeforeRead = modifiedTime(path);
    FileReadResult read = FileService::readAll(path);
    if (!read.ok()) {
        outcome.failure = OpenFailure::File;
        outcome.fileError = read.error;
        outcome.systemMessage = read.systemMessage;
        return outcome;
    }

    DecodeResult decoded = EncodingService::decodeUtf8(read.data);
    if (!decoded.ok()) {
        outcome.failure = decoded.error == DecodeError::ContainsNul ? OpenFailure::Binary
                                                                     : OpenFailure::InvalidUtf8;
        return outcome;
    }

    outcome.status = OpenStatus::Opened;
    outcome.id = m_documents.addFile(info.absoluteFilePath(), canonical, decoded.hasBom,
                                     decoded.lineEnding);
    if (DocumentState *document = m_documents.find(outcome.id)) {
        document->language = LanguageRegistry::detect(path, firstLine(decoded.text));
        document->indentWithTabs = Indentation::usesTabs(decoded.text);
        document->disk = snapshotOf(read.data, modifiedBeforeRead);
    }
    outcome.text = std::move(decoded.text);
    return outcome;
}

SaveOutcome DocumentController::save(DocumentId id, const QString &text, bool overwriteDiskChanges)
{
    DocumentState *document = m_documents.find(id);
    if (!document)
        return {SaveFailure::UnknownDocument, FileError::None, {}};
    if (document->isUntitled())
        return {SaveFailure::NoPath, FileError::None, {}};
    // Ścieżka destrukcyjna: zawsze pełne porównanie treści (rozmiar i mtime nie wystarczą).
    if (!overwriteDiskChanges && checkDisk(id, false) == DiskStatus::Changed)
        return {SaveFailure::DiskChanged, FileError::None, {}};
    return writeDocument(*document, document->filePath, text);
}

SaveOutcome DocumentController::saveAs(DocumentId id, const QString &path, const QString &text)
{
    DocumentState *document = m_documents.find(id);
    if (!document)
        return {SaveFailure::UnknownDocument, FileError::None, {}};

    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (const auto other = m_documents.findByCanonicalPath(canonical); other && *other != id)
        return {SaveFailure::OpenInAnotherDocument, FileError::None, {}};

    return writeDocument(*document, QFileInfo(path).absoluteFilePath(), text);
}

void DocumentController::close(DocumentId id)
{
    m_documents.remove(id);
}

DiskStatus DocumentController::checkDisk(DocumentId id, bool trustMetadata)
{
    DocumentState *document = m_documents.find(id);
    if (!document || document->isUntitled() || !document->disk.known)
        return DiskStatus::Unchanged;

    const QFileInfo info(document->filePath);
    if (!info.exists()) {
        document->missingOnDisk = true;
        return DiskStatus::Missing;
    }
    document->missingOnDisk = false;

    const bool sameMetadata =
        info.size() == document->disk.size && info.lastModified() == document->disk.modified;
    if (trustMetadata && sameMetadata)
        return DiskStatus::Unchanged;

    const QDateTime modifiedBeforeRead = info.lastModified();
    const FileReadResult read = FileService::readAll(document->filePath);
    if (!read.ok())
        return DiskStatus::Changed;
    const QByteArray hash = sha256(read.data);
    if (hash == document->disk.sha256) {
        // Ta sama treść, inne metadane (np. touch) — bez pytania użytkownika.
        if (!sameMetadata)
            document->disk = snapshotOf(read.data, modifiedBeforeRead);
        return DiskStatus::Unchanged;
    }
    document->lastSeenDiskSha256 = hash;
    return DiskStatus::Changed;
}

void DocumentController::declineDiskVersion(DocumentId id)
{
    if (DocumentState *document = m_documents.find(id))
        document->declinedDiskSha256 = document->lastSeenDiskSha256;
}

bool DocumentController::isDiskVersionDeclined(DocumentId id) const
{
    const DocumentState *document = m_documents.find(id);
    return document && !document->declinedDiskSha256.isEmpty()
           && document->declinedDiskSha256 == document->lastSeenDiskSha256;
}

DocumentController::ReloadData DocumentController::prepareReload(DocumentId id) const
{
    ReloadData data;
    const DocumentState *document = m_documents.find(id);
    if (!document || document->isUntitled()) {
        data.outcome.failure = OpenFailure::File;
        data.outcome.fileError = FileError::NotFound;
        return data;
    }

    const QDateTime modifiedBeforeRead = modifiedTime(document->filePath);
    FileReadResult read = FileService::readAll(document->filePath);
    if (!read.ok()) {
        data.outcome.failure = OpenFailure::File;
        data.outcome.fileError = read.error;
        data.outcome.systemMessage = read.systemMessage;
        return data;
    }
    DecodeResult decoded = EncodingService::decodeUtf8(read.data);
    if (!decoded.ok()) {
        data.outcome.failure = decoded.error == DecodeError::ContainsNul ? OpenFailure::Binary
                                                                          : OpenFailure::InvalidUtf8;
        return data;
    }

    data.state.hasBom = decoded.hasBom;
    data.state.lineEnding = decoded.lineEnding;
    data.state.indentWithTabs = Indentation::usesTabs(decoded.text);
    data.state.disk = snapshotOf(read.data, modifiedBeforeRead);
    data.outcome.status = OpenStatus::Opened;
    data.outcome.id = id;
    data.outcome.text = std::move(decoded.text);
    return data;
}

void DocumentController::commitReload(DocumentId id, const ReloadData &data)
{
    DocumentState *document = m_documents.find(id);
    if (!document || data.outcome.status != OpenStatus::Opened)
        return;
    document->hasBom = data.state.hasBom;
    document->lineEnding = data.state.lineEnding;
    document->indentWithTabs = data.state.indentWithTabs;
    document->disk = data.state.disk;
    document->missingOnDisk = false;
    document->lastSeenDiskSha256.clear();
    document->declinedDiskSha256.clear();
}

OpenOutcome DocumentController::reload(DocumentId id)
{
    ReloadData data = prepareReload(id);
    commitReload(id, data);
    return data.outcome;
}

void DocumentController::setLanguage(DocumentId id, LanguageId language)
{
    if (DocumentState *document = m_documents.find(id)) {
        document->language = language;
        document->languageChosenManually = true;
    }
}

void DocumentController::setLineEnding(DocumentId id, LineEnding lineEnding)
{
    if (DocumentState *document = m_documents.find(id))
        document->lineEnding = lineEnding;
}

SaveOutcome DocumentController::writeDocument(DocumentState &document, const QString &path,
                                              const QString &text)
{
    const QByteArray bytes = EncodingService::encodeUtf8(text, document.hasBom);
    const FileWriteResult written = FileService::writeAtomically(path, bytes);
    if (!written.ok())
        return {SaveFailure::File, written.error, written.systemMessage};

    // Metadane zmieniamy dopiero po potwierdzonym zapisie.
    document.filePath = path;
    document.canonicalPath = QFileInfo(path).canonicalFilePath();
    document.untitledNumber = 0;
    // Czas po zapisie; ewentualną zmianę tuż po commit() wykryje pełne porównanie
    // skrótu przy zdarzeniu obserwatora i przy następnym zapisie.
    document.disk = snapshotOf(bytes, modifiedTime(path));
    document.missingOnDisk = false;
    document.lastSeenDiskSha256.clear();
    document.declinedDiskSha256.clear();
    if (!document.languageChosenManually)
        document.language = LanguageRegistry::detect(path, firstLine(text));
    return {};
}

} // namespace cynamoncode
