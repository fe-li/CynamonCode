#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <array>
#include <optional>

namespace cynamoncode {

// Sesja (D034): tylko ścieżki i pozycje — nigdy treść dokumentów.
struct SessionTab
{
    QString path;
    int line = 0;
    int index = 0;
    int firstVisibleLine = 0;
    QString languageKey; // ręcznie wybrany język (LanguageRegistry::key); pusty = wykrywanie

    friend bool operator==(const SessionTab &, const SessionTab &) = default;
};

struct SessionPane
{
    QList<SessionTab> tabs;
    int current = -1;

    friend bool operator==(const SessionPane &, const SessionPane &) = default;
};

struct Session
{
    std::array<SessionPane, 2> panes;
    int activePane = 0;
    QString workspaceFolder;
    QList<int> splitterSizes;

    friend bool operator==(const Session &, const Session &) = default;
};

namespace SessionService {

inline constexpr int FormatVersion = 1;

QByteArray toJson(const Session &session);
// nullopt dla uszkodzonego lub nieznanego formatu (sesja jest wtedy pomijana).
std::optional<Session> fromJson(const QByteArray &json);

// Odczyt pliku; brak pliku = nullopt bez błędu.
std::optional<Session> load(const QString &path);
// Zapis atomowy (QSaveFile); false przy błędzie — sesja nie jest krytyczna.
bool save(const QString &path, const Session &session);

} // namespace SessionService

} // namespace cynamoncode
