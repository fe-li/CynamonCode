// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QByteArray>
#include <QString>

namespace cynamoncode {

enum class FileError {
    None,
    NotFound,
    NotRegularFile,
    NotReadable,
    NotWritable,   // istniejący plik bez prawa zapisu (D017)
    BrokenSymlink, // dowiązanie wskazuje na nieistniejący plik
    OpenFailed,    // np. brak prawa tworzenia pliku tymczasowego w katalogu
    ReadFailed,
    WriteFailed,
    CommitFailed,
};

struct FileReadResult
{
    FileError error = FileError::None;
    QString systemMessage; // komunikat Qt/systemu do wyświetlenia w szczegółach
    QByteArray data;

    bool ok() const { return error == FileError::None; }
};

struct FileWriteResult
{
    FileError error = FileError::None;
    QString systemMessage;

    bool ok() const { return error == FileError::None; }
};

// Operacje na surowych bajtach. Bez dialogów i bez dekodowania tekstu.
namespace FileService {

FileReadResult readAll(const QString &path);

// Zapis całego pliku przez QSaveFile (bez directWriteFallback). Przy błędzie
// istniejący plik pozostaje nietknięty. Zapis przez dowiązanie symboliczne
// trafia do pliku docelowego, a dowiązanie zostaje zachowane.
FileWriteResult writeAtomically(const QString &path, const QByteArray &data);

} // namespace FileService

} // namespace cynamoncode
