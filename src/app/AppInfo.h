// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QString>

// Dane programu w jednym miejscu (okno „O programie”, tytuł, QApplication).
// CYNAMONCODE_VERSION pochodzi z project(VERSION …) w CMakeLists.txt.
namespace cynamoncode::AppInfo {

// Nazwa produktu, autor i adres nie podlegają tłumaczeniu.
inline QString productName()
{
    return QStringLiteral("CynamonCode");
}

inline QString version()
{
    return QStringLiteral(CYNAMONCODE_VERSION);
}

inline QString author()
{
    return QStringLiteral("Kamil Feliszewski");
}

inline QString website()
{
    return QStringLiteral("https://www.feliszewski.pl");
}

// Identyfikator aplikacji reverse-DNS (D041): nazwa pliku .desktop, metadanych
// AppStream i ikony w motywie hicolor.
inline QString applicationId()
{
    return QStringLiteral("pl.feliszewski.CynamonCode");
}

// Identyfikator SPDX licencji programu (D040); pełny tekst w pliku LICENSE.
inline QString licenseSpdx()
{
    return QStringLiteral("GPL-3.0-or-later");
}

} // namespace cynamoncode::AppInfo
