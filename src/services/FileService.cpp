// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/FileService.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>

namespace cynamoncode::FileService {

FileReadResult readAll(const QString &path)
{
    FileReadResult result;
    const QFileInfo info(path);

    if (!info.exists()) {
        result.error = info.isSymbolicLink() ? FileError::BrokenSymlink : FileError::NotFound;
        return result;
    }
    if (!info.isFile()) {
        result.error = FileError::NotRegularFile;
        return result;
    }
    if (!info.isReadable()) {
        result.error = FileError::NotReadable;
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = FileError::OpenFailed;
        result.systemMessage = file.errorString();
        return result;
    }

    result.data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        result.error = FileError::ReadFailed;
        result.systemMessage = file.errorString();
        result.data.clear();
    }
    return result;
}

namespace {

// Jedna próba zapisu atomowego. directWriteFallback pozostaje wyłączony (domyślnie
// false): bez możliwości atomowej podmiany zapis ma się nie udać, zamiast
// nadpisywać plik w miejscu.
FileWriteResult writeOnce(const QString &target, const QByteArray &data)
{
    FileWriteResult result;
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = FileError::OpenFailed;
        result.systemMessage = file.errorString();
        return result;
    }

    const qint64 written = file.write(data);
    if (written != data.size()) {
        result.error = FileError::WriteFailed;
        result.systemMessage = file.errorString();
        file.cancelWriting();
        return result;
    }

    if (!file.commit()) {
        result.error = FileError::CommitFailed;
        result.systemMessage = file.errorString();
    }
    return result;
}

} // namespace

FileWriteResult writeAtomically(const QString &path, const QByteArray &data)
{
    FileWriteResult result;

    QString target = path;
    const QFileInfo info(path);
    // Tylko dowiązania symboliczne — skrót Windows (.lnk) to zwykły plik (D053).
    if (info.isSymbolicLink()) {
        const QString resolved = info.canonicalFilePath();
        if (resolved.isEmpty()) {
            result.error = FileError::BrokenSymlink;
            return result;
        }
        target = resolved;
    }

    const QFileInfo targetInfo(target);
    if (targetInfo.exists()) {
        if (!targetInfo.isFile()) {
            result.error = FileError::NotRegularFile;
            return result;
        }
        if (!targetInfo.isWritable()) {
            result.error = FileError::NotWritable;
            return result;
        }
    }

    result = writeOnce(target, data);
#ifdef Q_OS_WIN
    // Windows: plik świeżo zapisany lub odczytany bywa przez chwilę otwarty przez
    // inny proces (Windows Defender, indeksowanie) i podmiana kończy się „Odmową
    // dostępu”. Oryginał jest wtedy nietknięty — ponawiamy cały zapis atomowy.
    for (const int delayMs : {25, 50, 100, 150, 200, 250, 300}) {
        if (result.error != FileError::CommitFailed)
            break;
        QThread::msleep(static_cast<unsigned long>(delayMs));
        result = writeOnce(target, data);
    }
#endif
    return result;
}

} // namespace cynamoncode::FileService
