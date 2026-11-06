// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "services/SessionService.h"

#include "services/FileService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace cynamoncode::SessionService {

namespace {

QJsonObject tabToJson(const SessionTab &tab)
{
    QJsonObject object;
    object.insert(QStringLiteral("path"), tab.path);
    object.insert(QStringLiteral("line"), tab.line);
    object.insert(QStringLiteral("index"), tab.index);
    object.insert(QStringLiteral("firstVisibleLine"), tab.firstVisibleLine);
    if (!tab.languageKey.isEmpty())
        object.insert(QStringLiteral("language"), tab.languageKey);
    return object;
}

int nonNegative(const QJsonValue &value)
{
    return std::max(0, value.toInt());
}

} // namespace

QByteArray toJson(const Session &session)
{
    QJsonArray panes;
    for (const SessionPane &pane : session.panes) {
        QJsonArray tabs;
        for (const SessionTab &tab : pane.tabs)
            tabs.append(tabToJson(tab));
        QJsonObject paneObject;
        paneObject.insert(QStringLiteral("tabs"), tabs);
        paneObject.insert(QStringLiteral("current"), pane.current);
        panes.append(paneObject);
    }

    QJsonArray sizes;
    for (const int size : session.splitterSizes)
        sizes.append(size);

    QJsonObject root;
    root.insert(QStringLiteral("version"), FormatVersion);
    root.insert(QStringLiteral("panes"), panes);
    root.insert(QStringLiteral("activePane"), session.activePane);
    root.insert(QStringLiteral("workspaceFolder"), session.workspaceFolder);
    root.insert(QStringLiteral("splitterSizes"), sizes);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<Session> fromJson(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != FormatVersion)
        return std::nullopt;

    Session session;
    const QJsonArray panes = root.value(QStringLiteral("panes")).toArray();
    for (qsizetype p = 0; p < std::min<qsizetype>(panes.size(), 2); ++p) {
        const QJsonObject paneObject = panes.at(p).toObject();
        SessionPane &pane = session.panes[static_cast<size_t>(p)];
        for (const QJsonValue &value : paneObject.value(QStringLiteral("tabs")).toArray()) {
            const QJsonObject object = value.toObject();
            SessionTab tab;
            tab.path = object.value(QStringLiteral("path")).toString();
            if (tab.path.isEmpty())
                continue;
            tab.line = nonNegative(object.value(QStringLiteral("line")));
            tab.index = nonNegative(object.value(QStringLiteral("index")));
            tab.firstVisibleLine = nonNegative(object.value(QStringLiteral("firstVisibleLine")));
            tab.languageKey = object.value(QStringLiteral("language")).toString();
            pane.tabs.append(tab);
        }
        const int current = paneObject.value(QStringLiteral("current")).toInt(-1);
        pane.current = (current >= 0 && current < pane.tabs.size()) ? current : -1;
    }
    session.activePane = root.value(QStringLiteral("activePane")).toInt() == 1 ? 1 : 0;
    session.workspaceFolder = root.value(QStringLiteral("workspaceFolder")).toString();
    for (const QJsonValue &value : root.value(QStringLiteral("splitterSizes")).toArray())
        session.splitterSizes.append(std::max(0, value.toInt()));
    return session;
}

std::optional<Session> load(const QString &path)
{
    if (!QFile::exists(path))
        return std::nullopt;
    const FileReadResult read = FileService::readAll(path);
    if (!read.ok())
        return std::nullopt;
    return fromJson(read.data);
}

bool save(const QString &path, const Session &session)
{
    // Przy pierwszym uruchomieniu katalog ustawień może jeszcze nie istnieć.
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    return FileService::writeAtomically(path, toJson(session)).ok();
}

} // namespace cynamoncode::SessionService
