#include "controllers/DocumentController.h"

#include "services/EncodingService.h"
#include "services/Indentation.h"
#include "services/LanguageRegistry.h"

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
    }
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
    const FileWriteResult written =
        FileService::writeAtomically(path, EncodingService::encodeUtf8(text, document.hasBom));
    if (!written.ok())
        return {SaveFailure::File, written.error, written.systemMessage};

    // Metadane zmieniamy dopiero po potwierdzonym zapisie.
    document.filePath = path;
    document.canonicalPath = QFileInfo(path).canonicalFilePath();
    document.untitledNumber = 0;
    if (!document.languageChosenManually)
        document.language = LanguageRegistry::detect(path, firstLine(text));
    return {};
}

} // namespace cynamoncode
