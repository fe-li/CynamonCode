#include "controllers/DocumentController.h"

#include "services/EncodingService.h"

#include <QFileInfo>

#include <utility>

namespace cynamoncode {

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
    outcome.text = std::move(decoded.text);
    return outcome;
}

SaveOutcome DocumentController::save(DocumentId id, const QString &text)
{
    DocumentState *document = m_documents.find(id);
    if (!document)
        return {SaveFailure::UnknownDocument, FileError::None, {}};
    if (document->isUntitled())
        return {SaveFailure::NoPath, FileError::None, {}};
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

SaveOutcome DocumentController::writeDocument(DocumentState &document, const QString &path,
                                              const QString &text)
{
    const FileWriteResult written =
        FileService::writeAtomically(path, EncodingService::encodeUtf8(text, document.hasBom));
    if (!written.ok())
        return {SaveFailure::File, written.error, written.systemMessage};

    // Metadane zmieniamy dopiero po potwierdzonym zapisie.
    document.filePath = path;
    document.canonicalPath = QFileInfo(path).canonicalFilePath();
    document.untitledNumber = 0;
    return {};
}

} // namespace cynamoncode
