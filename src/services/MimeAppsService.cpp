// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/MimeAppsService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>

#include <memory>
#include <utility>
#include <vector>

namespace cynamoncode {

namespace {

const QString DefaultsGroup = QStringLiteral("Default Applications");
const QByteArray Utf8Bom("\xEF\xBB\xBF");

bool groupHeader(const QString &line, QString *name)
{
    const QString trimmed = line.trimmed();
    if (trimmed.size() < 2 || !trimmed.startsWith(QLatin1Char('['))
        || !trimmed.endsWith(QLatin1Char(']')))
        return false;
    *name = trimmed.mid(1, trimmed.size() - 2);
    return true;
}

bool keyLine(const QString &line, QString *key, QString *value)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
        return false;
    const qsizetype separator = trimmed.indexOf(QLatin1Char('='));
    if (separator <= 0)
        return false;
    *key = trimmed.left(separator).trimmed();
    *value = trimmed.mid(separator + 1).trimmed();
    return true;
}

QStringList splitList(const QString &value)
{
    QStringList result;
    for (const QString &part : value.split(QLatin1Char(';'))) {
        const QString item = part.trimmed();
        if (!item.isEmpty() && !result.contains(item))
            result.append(item);
    }
    return result;
}

QString entryLine(const QString &mimeType, const QStringList &applications)
{
    return mimeType + QLatin1Char('=') + applications.join(QLatin1Char(';')) + QLatin1Char(';');
}

// Położenie wpisu w treści. Jak GKeyFile (GLib, z którego korzystają Cinnamon
// i Nemo): wszystkie grupy [Default Applications] tworzą jedną, a z powtórzonych
// kluczy obowiązuje ostatni.
struct EntryLocation
{
    qsizetype lastGroupStart = -1;
    qsizetype entry = -1;
};

EntryLocation locate(const QStringList &lines, const QString &mimeType)
{
    EntryLocation location;
    bool inGroup = false;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        QString name;
        if (groupHeader(lines.at(i), &name)) {
            inGroup = name == DefaultsGroup;
            if (inGroup)
                location.lastGroupStart = i;
            continue;
        }
        QString key;
        QString value;
        if (inGroup && keyLine(lines.at(i), &key, &value) && key == mimeType)
            location.entry = i;
    }
    return location;
}

QStringList splitLines(const QString &content)
{
    QStringList lines = content.split(QLatin1Char('\n'));
    if (content.endsWith(QLatin1Char('\n')))
        lines.removeLast();
    if (content.isEmpty())
        lines.clear();
    return lines;
}

// Treść pliku: tylko poprawny UTF-8 (bez cichej zamiany bajtów na U+FFFD);
// znacznik BOM jest zapamiętywany i odtwarzany przy zapisie.
struct FileText
{
    QString text;
    bool bom = false;
};

bool readText(const QString &path, FileText *result, QString *error)
{
    *result = FileText{};
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString() + QStringLiteral(": ") + path;
        return false;
    }
    QByteArray bytes = file.readAll();
    if (bytes.startsWith(Utf8Bom)) {
        result->bom = true;
        bytes.remove(0, Utf8Bom.size());
    }
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    result->text = decoder(bytes);
    if (decoder.hasError()) {
        if (error)
            *error = QStringLiteral("plik nie jest w UTF-8 — nie został zmieniony: ") + path;
        return false;
    }
    return true;
}

} // namespace

namespace MimeApps {

QStringList defaultsFor(const QString &content, const QString &mimeType)
{
    const QStringList lines = splitLines(content);
    const EntryLocation location = locate(lines, mimeType);
    if (location.entry < 0)
        return {};
    QString key;
    QString value;
    keyLine(lines.at(location.entry), &key, &value);
    return splitList(value);
}

QString withDefault(const QString &content, const QString &mimeType, const QString &desktopId,
                    bool enabled)
{
    QStringList lines = splitLines(content);
    const EntryLocation location = locate(lines, mimeType);
    const QStringList applications = defaultsFor(content, mimeType);
    QStringList updated = applications;
    updated.removeAll(desktopId);
    if (enabled)
        updated.prepend(desktopId);
    if (updated == applications)
        return content; // nic do zmiany — także bez normalizacji wierszy

    // Końce wierszy w stylu pliku: przy CRLF wiersze zachowują własne „\r”,
    // a nowe i zmienione dostają je tu (łączone są przez „\n”).
    const bool crlf = content.contains(QStringLiteral("\r\n"));
    const QString newLine = entryLine(mimeType, updated);
    if (crlf && !lines.isEmpty() && !lines.constLast().endsWith(QLatin1Char('\r')))
        lines.last() += QLatin1Char('\r'); // plik CRLF bez końcowego znaku nowej linii

    if (location.entry >= 0) {
        // Zmieniany jest tylko obowiązujący (ostatni) wpis; wcześniejsze, i tak
        // nieobowiązujące, zostają bez zmian.
        if (updated.isEmpty())
            lines.removeAt(location.entry);
        else
            lines[location.entry] = newLine + (crlf ? QStringLiteral("\r") : QString());
    } else if (location.lastGroupStart >= 0) {
        lines.insert(location.lastGroupStart + 1,
                     newLine + (crlf ? QStringLiteral("\r") : QString()));
    } else {
        if (!lines.isEmpty() && !lines.constLast().trimmed().isEmpty())
            lines.append(crlf ? QStringLiteral("\r") : QString());
        lines.append(QLatin1Char('[') + DefaultsGroup + QLatin1Char(']')
                     + (crlf ? QStringLiteral("\r") : QString()));
        lines.append(newLine + (crlf ? QStringLiteral("\r") : QString()));
    }
    if (lines.isEmpty())
        return QString();
    QString result = lines.join(QLatin1Char('\n'));
    if (crlf && !result.endsWith(QLatin1Char('\r')))
        result += QLatin1Char('\r');
    return result + QLatin1Char('\n');
}

} // namespace MimeApps

MimeAppsService::MimeAppsService(QStringList files, QString desktopId)
    : m_files(std::move(files))
    , m_desktopId(std::move(desktopId))
{
}

QStringList MimeAppsService::userFiles()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QStringList files;
    const QStringList desktops =
        qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &desktop : desktops) {
        const QString name = desktop.trimmed().toLower();
        if (!name.isEmpty() && !name.contains(QLatin1Char('/')))
            files.append(QDir(config).filePath(name + QStringLiteral("-mimeapps.list")));
    }
    files.append(QDir(config).filePath(QStringLiteral("mimeapps.list")));
    return files;
}

bool MimeAppsService::isDefault(const QString &mimeType) const
{
    for (const QString &path : m_files) {
        FileText content;
        if (!readText(path, &content, nullptr))
            continue;
        const QStringList applications = MimeApps::defaultsFor(content.text, mimeType);
        if (!applications.isEmpty())
            return applications.constFirst() == m_desktopId;
    }
    return false;
}

bool MimeAppsService::apply(const QStringList &enable, const QStringList &disable, QString *error)
{
    // Najpierw odczyt i przygotowanie wszystkich plików: błąd odczytu lub
    // kodowania przerywa całość, zanim cokolwiek zostanie zapisane.
    struct Pending
    {
        QString path;
        QByteArray bytes;
    };
    std::vector<Pending> pending;
    for (qsizetype i = 0; i < m_files.size(); ++i) {
        const QString &path = m_files.at(i);
        const bool primary = i == m_files.size() - 1;
        if (!primary && !QFileInfo::exists(path))
            continue;

        FileText content;
        if (!readText(path, &content, error))
            return false;
        QString updated = content.text;
        for (const QString &mimeType : enable)
            updated = MimeApps::withDefault(updated, mimeType, m_desktopId, true);
        for (const QString &mimeType : disable)
            updated = MimeApps::withDefault(updated, mimeType, m_desktopId, false);
        if (updated == content.text)
            continue;
        pending.push_back({path, (content.bom ? Utf8Bom : QByteArray()) + updated.toUtf8()});
    }

    // Wszystkie pliki tymczasowe zapisane przed pierwszym commit() — błąd zapisu
    // (np. brak miejsca) nie zostawia stanu mieszanego.
    std::vector<std::unique_ptr<QSaveFile>> files;
    for (const Pending &item : pending) {
        QDir().mkpath(QFileInfo(item.path).absolutePath());
        auto file = std::make_unique<QSaveFile>(item.path);
        if (!file->open(QIODevice::WriteOnly) || file->write(item.bytes) != item.bytes.size()) {
            if (error)
                *error = file->errorString() + QStringLiteral(": ") + item.path;
            for (const auto &opened : files)
                opened->cancelWriting();
            file->cancelWriting();
            return false;
        }
        files.push_back(std::move(file));
    }
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (!files[i]->commit()) {
            if (error)
                *error = files[i]->errorString() + QStringLiteral(": ") + pending[i].path;
            for (std::size_t j = i + 1; j < files.size(); ++j)
                files[j]->cancelWriting();
            return false;
        }
    }
    return true;
}

} // namespace cynamoncode
