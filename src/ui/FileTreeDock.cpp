// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/FileTreeDock.h"

#include <QDir>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeView>
#include <QVBoxLayout>

namespace cynamoncode {

FileTreeDock::FileTreeDock(QWidget *parent)
    : QDockWidget(tr("Pliki"), parent)
{
    setObjectName(QStringLiteral("fileTreeDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_model->setNameFilterDisables(false); // niepasujące pliki są ukrywane, nie wyszarzane
    m_model->setReadOnly(true);            // panel nie zmienia nazw ani nie usuwa plików

    m_filter = new QLineEdit;
    m_filter->setObjectName(QStringLiteral("fileFilterEdit"));
    m_filter->setPlaceholderText(tr("Filtruj nazwy plików…"));
    m_filter->setClearButtonEnabled(true);

    m_view = new QTreeView;
    m_view->setObjectName(QStringLiteral("fileTreeView"));
    m_view->setModel(m_model);
    m_view->setHeaderHidden(true);
    for (int column = 1; column < m_model->columnCount(); ++column)
        m_view->hideColumn(column); // tylko nazwa
    m_view->setUniformRowHeights(true);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_filter);
    layout->addWidget(m_view);
    setWidget(content);

    connect(m_filter, &QLineEdit::textChanged, this, &FileTreeDock::setFilter);
    // activated = dwuklik lub Enter (zależnie od ustawień platformy).
    connect(m_view, &QTreeView::activated, this, &FileTreeDock::activate);
}

void FileTreeDock::setRootFolder(const QString &folder)
{
    m_root = QDir(folder).absolutePath();
    m_view->setRootIndex(m_model->setRootPath(m_root));
    setWindowTitle(tr("Pliki — %1").arg(QDir(m_root).dirName()));
    setToolTip(QDir::toNativeSeparators(m_root));
}

void FileTreeDock::setFilter(const QString &text)
{
    if (m_filter->text() != text)
        m_filter->setText(text);
    const QString trimmed = text.trimmed();
    m_model->setNameFilters(trimmed.isEmpty() ? QStringList()
                                              : QStringList{QLatin1Char('*') + trimmed
                                                            + QLatin1Char('*')});
}

void FileTreeDock::activate(const QModelIndex &index)
{
    if (!index.isValid() || m_model->isDir(index))
        return; // foldery rozwija widok
    emit fileActivated(m_model->filePath(index));
}

} // namespace cynamoncode
