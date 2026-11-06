// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "domain/DocumentState.h"

#include <memory>
#include <optional>
#include <vector>

namespace cynamoncode {

// Rejestr otwartych dokumentów (metadane, bez tekstu i bez widżetów).
// Wskaźniki zwracane przez find() pozostają ważne do usunięcia danego
// dokumentu — dodawanie kolejnych ich nie unieważnia.
class DocumentManager
{
public:
    DocumentId createUntitled();
    DocumentId addFile(const QString &filePath, const QString &canonicalPath, bool hasBom,
                       LineEnding lineEnding);

    const DocumentState *find(DocumentId id) const;
    DocumentState *find(DocumentId id);
    std::optional<DocumentId> findByCanonicalPath(const QString &canonicalPath) const;

    bool remove(DocumentId id);
    qsizetype count() const { return static_cast<qsizetype>(m_documents.size()); }

private:
    DocumentId add(DocumentState state);
    int lowestFreeUntitledNumber() const;

    std::vector<std::unique_ptr<DocumentState>> m_documents;
    DocumentId m_nextId = 1;
};

} // namespace cynamoncode
