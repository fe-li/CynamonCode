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

} // namespace cynamoncode::AppInfo
