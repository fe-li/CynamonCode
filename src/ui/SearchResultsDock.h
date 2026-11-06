// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "domain/DocumentState.h"

#include <QDockWidget>
#include <QList>
#include <QString>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace cynamoncode {

struct SearchHit
{
    qsizetype start = 0;
    qsizetype length = 0;
    int line = 0; // od 0
    QString preview;
};

// Wyniki „Znajdź we wszystkich otwartych” (D038): dokument → trafienia;
// aktywacja trafienia zgłasza dokument i pozycję.
class SearchResultsDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit SearchResultsDock(QWidget *parent = nullptr);

    void clearResults(const QString &summary);
    // `totalHits` — wszystkie trafienia w dokumencie (może być więcej niż `hits`).
    void addDocument(DocumentId id, const QString &name, int totalHits,
                     const QList<SearchHit> &hits);
    void setSummary(const QString &summary);

    QTreeWidget *tree() const { return m_tree; }

signals:
    void hitActivated(DocumentId id, qsizetype start, qsizetype length);

private:
    void activate(QTreeWidgetItem *item);

    QLabel *m_summary = nullptr;
    QTreeWidget *m_tree = nullptr;
};

} // namespace cynamoncode
