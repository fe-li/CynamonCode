// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

// Różnice systemowe w testach (Linux, Windows, macOS).

#include <QString>
#include <QtGlobal>

#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace cynamoncode::testing {

// Czy odebranie praw zapisu katalogowi (QFile::setPermissions) blokuje tworzenie
// w nim plików. Nie: root na Uniksie oraz Windows (brak praw POSIX dla katalogów).
inline bool permissionsEnforced()
{
#ifdef Q_OS_UNIX
    return ::geteuid() != 0;
#else
    return false;
#endif
}

// Czy plik bez prawa zapisu odrzuca zapis. Windows: atrybut „tylko do odczytu”.
inline bool readOnlyFilesEnforced()
{
#ifdef Q_OS_WIN
    return true;
#else
    return permissionsEnforced();
#endif
}

// Czy QFile::link tworzy dowiązanie symboliczne (na Windows tworzy skrót .lnk).
inline bool symlinksAvailable()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

// Atomowa podmiana pliku przez rename — także gdy plik docelowy istnieje
// (std::rename na Windows wtedy odmawia). Na Windows świeżo zapisany plik bywa
// chwilę otwarty przez inny proces (skanowanie) — kilka ponowień jak w FileService.
inline bool replaceByRename(const QString &from, const QString &to)
{
    const std::filesystem::path source(from.toStdU16String());
    const std::filesystem::path target(to.toStdU16String());
    std::error_code error;
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::filesystem::rename(source, target, error);
        if (!error)
            return true;
#ifdef Q_OS_WIN
        std::this_thread::sleep_for(std::chrono::milliseconds(50 * (attempt + 1)));
#else
        return false;
#endif
    }
    return false;
}

} // namespace cynamoncode::testing
