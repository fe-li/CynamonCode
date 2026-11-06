// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QDockWidget>
#include <QString>

class QFileSystemModel;
class QLineEdit;
class QModelIndex;
class QTreeView;

namespace cynamoncode {

// Panel plików (D037): drzewo wybranego folderu z filtrem nazw. Aktywacja pliku
// (dwuklik / Enter) zgłasza ścieżkę do otwarcia w edytorze — nic nie jest uruchamiane.
class FileTreeDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit FileTreeDock(QWidget *parent = nullptr);

    void setRootFolder(const QString &folder);
    QString rootFolder() const { return m_root; }

    // Filtr nazw plików (fragment nazwy, bez rozróżniania wielkości liter).
    void setFilter(const QString &text);

    QTreeView *view() const { return m_view; }
    QFileSystemModel *model() const { return m_model; }

signals:
    void fileActivated(const QString &path);

private:
    void activate(const QModelIndex &index);

    QString m_root;
    QFileSystemModel *m_model = nullptr;
    QTreeView *m_view = nullptr;
    QLineEdit *m_filter = nullptr;
};

} // namespace cynamoncode
