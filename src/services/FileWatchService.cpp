// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/FileWatchService.h"

#include <QFileInfo>

#include <utility>

namespace cynamoncode {

FileWatchService::FileWatchService(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(DebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &FileWatchService::flush);

    m_retry.setInterval(MissingRetryMs);
    connect(&m_retry, &QTimer::timeout, this, &FileWatchService::retryMissing);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (!m_paths.contains(path))
            return;
        m_pending.insert(path);
        // Bez restartu aktywnego okna — inaczej często zmieniany plik zagłodziłby resztę.
        if (!m_debounce.isActive())
            m_debounce.start();
    });
}

void FileWatchService::watch(const QString &path)
{
    if (path.isEmpty())
        return;
    m_paths.insert(path);
    rewatch(path);
}

void FileWatchService::unwatch(const QString &path)
{
    m_paths.remove(path);
    m_pending.remove(path);
    m_missing.remove(path);
    if (m_watcher.files().contains(path))
        m_watcher.removePath(path);
    updateRetryTimer();
}

void FileWatchService::rewatch(const QString &path)
{
    if (!m_paths.contains(path))
        return;
    if (m_watcher.files().contains(path))
        m_watcher.removePath(path);
    if (QFileInfo::exists(path) && m_watcher.addPath(path))
        m_missing.remove(path);
    else
        m_missing.insert(path);
    updateRetryTimer();
}

void FileWatchService::flush()
{
    const QSet<QString> pending = std::exchange(m_pending, {});
    for (const QString &path : pending) {
        if (!m_paths.contains(path))
            continue;
        rewatch(path);
        emit fileChanged(path);
    }
}

void FileWatchService::retryMissing()
{
    const QSet<QString> missing = m_missing;
    for (const QString &path : missing) {
        if (!QFileInfo::exists(path))
            continue;
        rewatch(path);
        if (!m_missing.contains(path))
            emit fileChanged(path); // plik wrócił — UI sprawdzi treść
    }
}

void FileWatchService::updateRetryTimer()
{
    if (m_missing.isEmpty())
        m_retry.stop();
    else if (!m_retry.isActive())
        m_retry.start();
}

} // namespace cynamoncode
