// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/DocumentManager.h"

#include <algorithm>
#include <utility>

namespace cynamoncode {

DocumentId DocumentManager::createUntitled()
{
    DocumentState state;
    state.untitledNumber = lowestFreeUntitledNumber();
    state.lineEnding = LineEnding::LF;
    return add(std::move(state));
}

DocumentId DocumentManager::addFile(const QString &filePath, const QString &canonicalPath,
                                    bool hasBom, LineEnding lineEnding)
{
    DocumentState state;
    state.filePath = filePath;
    state.canonicalPath = canonicalPath;
    state.hasBom = hasBom;
    state.lineEnding = lineEnding;
    return add(std::move(state));
}

const DocumentState *DocumentManager::find(DocumentId id) const
{
    for (const auto &document : m_documents) {
        if (document->id == id)
            return document.get();
    }
    return nullptr;
}

DocumentState *DocumentManager::find(DocumentId id)
{
    for (const auto &document : m_documents) {
        if (document->id == id)
            return document.get();
    }
    return nullptr;
}

std::optional<DocumentId> DocumentManager::findByCanonicalPath(const QString &canonicalPath) const
{
    if (canonicalPath.isEmpty())
        return std::nullopt;
    for (const auto &document : m_documents) {
        if (document->canonicalPath == canonicalPath)
            return document->id;
    }
    return std::nullopt;
}

bool DocumentManager::remove(DocumentId id)
{
    const auto it = std::find_if(m_documents.begin(), m_documents.end(),
                                 [id](const auto &document) { return document->id == id; });
    if (it == m_documents.end())
        return false;
    m_documents.erase(it);
    return true;
}

DocumentId DocumentManager::add(DocumentState state)
{
    state.id = m_nextId++;
    m_documents.push_back(std::make_unique<DocumentState>(std::move(state)));
    return m_documents.back()->id;
}

int DocumentManager::lowestFreeUntitledNumber() const
{
    int candidate = 1;
    for (;;) {
        const bool taken = std::any_of(m_documents.begin(), m_documents.end(),
                                       [candidate](const auto &document) {
                                           return document->isUntitled()
                                                  && document->untitledNumber == candidate;
                                       });
        if (!taken)
            return candidate;
        ++candidate;
    }
}

} // namespace cynamoncode
