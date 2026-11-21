// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QString>

#include <optional>

namespace cynamoncode {

// Kodowanie pliku na dysku. UTF-8 jest domyślne; Windows-1250 tylko na wyraźny
// wybór użytkownika przy otwieraniu pliku spoza UTF-8 (D051).
enum class TextEncoding {
    Utf8,
    Windows1250,
};

// Klucz w pliku sesji.
inline QString textEncodingKey(TextEncoding encoding)
{
    return encoding == TextEncoding::Windows1250 ? QStringLiteral("windows-1250")
                                                 : QStringLiteral("utf-8");
}

inline std::optional<TextEncoding> textEncodingFromKey(const QString &key)
{
    if (key == QLatin1String("windows-1250"))
        return TextEncoding::Windows1250;
    if (key == QLatin1String("utf-8"))
        return TextEncoding::Utf8;
    return std::nullopt;
}

} // namespace cynamoncode
