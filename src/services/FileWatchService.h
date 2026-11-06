// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace cynamoncode {

// Obserwacja otwartych plików (D030).
// - Zdarzenia z krótkiego okna są łączone (zapis bywa serią zdarzeń); okno nie
//   jest przedłużane, więc ciągle zmieniany plik nie blokuje zgłoszeń innych.
// - Ścieżka jest ponownie podpinana, gdy plik podmieniono przez rename (QSaveFile,
//   git, inne edytory) — QFileSystemWatcher gubi wtedy obserwację.
// - Plik, który zniknął, jest sprawdzany okresowo; gdy wróci, obserwacja wraca
//   i zgłaszana jest zmiana.
class FileWatchService final : public QObject
{
    Q_OBJECT

public:
    explicit FileWatchService(QObject *parent = nullptr);

    void watch(const QString &path);
    void unwatch(const QString &path);
    // Po własnym zapisie (nowy i-węzeł).
    void rewatch(const QString &path);

    bool isWatching(const QString &path) const { return m_paths.contains(path); }

    static constexpr int DebounceMs = 150;
    static constexpr int MissingRetryMs = 1000;

signals:
    void fileChanged(const QString &path);

private:
    void flush();
    void retryMissing();
    void updateRetryTimer();

    QFileSystemWatcher m_watcher;
    QSet<QString> m_paths;
    QSet<QString> m_pending;
    QSet<QString> m_missing;
    QTimer m_debounce;
    QTimer m_retry;
};

} // namespace cynamoncode
