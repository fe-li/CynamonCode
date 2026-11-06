// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QListWidget;
class QListWidgetItem;

namespace cynamoncode {

class MimeAppsService;

// Grupa typów MIME pokazywana jako jedna pozycja (np. JavaScript = dwa typy).
struct FileTypeGroup
{
    QString name;
    QStringList mimeTypes;
};

// Ustawienia → „Domyślny program dla plików…” (D047). Pokazuje, dla których
// typów CynamonCode jest domyślnym programem bieżącego użytkownika; zmiany
// zapisuje okno główne po „OK”. Nic nie jest zaznaczane samoczynnie.
class DefaultAppsDialog final : public QDialog
{
    Q_OBJECT

public:
    // Typy z wpisu .desktop (MimeType=), pogrupowane według języków.
    static QList<FileTypeGroup> fileTypeGroups();

    DefaultAppsDialog(const MimeAppsService &service, bool installed, QWidget *parent = nullptr);

    QStringList groupNames() const;
    // Qt::Checked — wszystkie typy grupy otwiera CynamonCode, PartiallyChecked — część.
    Qt::CheckState checkState(const QString &group) const;
    bool setChecked(const QString &group, bool checked);

    // Wynik po „OK”: typy do ustawienia i do zwolnienia (tylko zmienione grupy).
    QStringList typesToEnable() const;
    QStringList typesToDisable() const;

private:
    QListWidgetItem *itemFor(const QString &group) const;

    QList<FileTypeGroup> m_groups;
    QList<Qt::CheckState> m_initial;
    QListWidget *m_list = nullptr;
};

} // namespace cynamoncode
