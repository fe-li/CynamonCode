// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/SearchResultsDock.h"

#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace cynamoncode {

namespace {
constexpr int DocumentRole = Qt::UserRole;
constexpr int StartRole = Qt::UserRole + 1;
constexpr int LengthRole = Qt::UserRole + 2;
} // namespace

SearchResultsDock::SearchResultsDock(QWidget *parent)
    : QDockWidget(tr("Wyniki wyszukiwania"), parent)
{
    setObjectName(QStringLiteral("searchResultsDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    m_summary = new QLabel;
    m_summary->setObjectName(QStringLiteral("searchSummaryLabel"));

    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("searchResultsTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Wiersz"), tr("Fragment")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->setUniformRowHeights(true);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_summary);
    layout->addWidget(m_tree);
    setWidget(content);

    // itemActivated = dwuklik lub Enter.
    connect(m_tree, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *item) { activate(item); });
}

void SearchResultsDock::clearResults(const QString &summary)
{
    m_tree->clear();
    setSummary(summary);
}

void SearchResultsDock::setSummary(const QString &summary)
{
    m_summary->setText(summary);
}

void SearchResultsDock::addDocument(DocumentId id, const QString &name, int totalHits,
                                    const QList<SearchHit> &hits)
{
    auto *documentItem = new QTreeWidgetItem(m_tree);
    documentItem->setText(0, tr("%1 (%2)").arg(name).arg(totalHits));
    documentItem->setFirstColumnSpanned(true);
    for (const SearchHit &hit : hits) {
        auto *item = new QTreeWidgetItem(documentItem);
        item->setText(0, QString::number(hit.line + 1));
        item->setText(1, hit.preview);
        item->setData(0, DocumentRole, QVariant::fromValue<quint64>(id));
        item->setData(0, StartRole, QVariant::fromValue<qlonglong>(hit.start));
        item->setData(0, LengthRole, QVariant::fromValue<qlonglong>(hit.length));
    }
    documentItem->setExpanded(true);
}

void SearchResultsDock::activate(QTreeWidgetItem *item)
{
    if (!item || !item->parent())
        return; // wiersz dokumentu
    emit hitActivated(item->data(0, DocumentRole).value<quint64>(),
                      item->data(0, StartRole).value<qlonglong>(),
                      item->data(0, LengthRole).value<qlonglong>());
}

} // namespace cynamoncode
