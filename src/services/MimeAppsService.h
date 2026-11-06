// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QString>
#include <QStringList>

namespace cynamoncode {

// Domyślny program dla typów plików (D047) — wyłącznie dla bieżącego użytkownika,
// w plikach mimeapps.list według specyfikacji freedesktop.org „MIME Applications
// Associations” (grupa [Default Applications]). Bez wywoływania zewnętrznych
// programów i bez zmian w plikach systemowych.
namespace MimeApps {

// Lista programów dla typu z grupy [Default Applications] (pusta, gdy brak wpisu).
QStringList defaultsFor(const QString &content, const QString &mimeType);

// Zwraca treść pliku po zmianie: `enabled` — `desktopId` na początku listy typu
// (pozostałe programy zostają za nim); wyłączenie usuwa tylko `desktopId`, więc
// wraca poprzedni wybór, a pusty wpis znika. Pozostałe wiersze bez zmian.
QString withDefault(const QString &content, const QString &mimeType, const QString &desktopId,
                    bool enabled);

} // namespace MimeApps

class MimeAppsService
{
public:
    // `files` w kolejności pierwszeństwa (np. x-cinnamon-mimeapps.list, potem
    // mimeapps.list); ostatni jest plikiem głównym, tworzonym w razie potrzeby.
    // Pozostałe są zmieniane tylko wtedy, gdy już istnieją.
    MimeAppsService(QStringList files, QString desktopId);

    // Pliki użytkownika według $XDG_CONFIG_HOME i $XDG_CURRENT_DESKTOP.
    static QStringList userFiles();

    const QString &desktopId() const { return m_desktopId; }

    // Czy program jest domyślny dla typu w ustawieniach użytkownika (pierwszy plik
    // z wpisem dla typu decyduje).
    bool isDefault(const QString &mimeType) const;

    // Ustawia lub zdejmuje program jako domyślny dla typów; zapis atomowy.
    // Zwraca false (z opisem błędu), gdy któregoś pliku nie udało się zapisać.
    bool apply(const QStringList &enable, const QStringList &disable, QString *error = nullptr);

private:
    QStringList m_files;
    QString m_desktopId;
};

} // namespace cynamoncode
