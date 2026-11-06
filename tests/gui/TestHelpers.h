// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

// Wspólne funkcje pomocnicze testów GUI.

#include "ScriptedPrompts.h"
#include "editor/EditorWidget.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QFile>
#include <QSettings>
#include <QtTest>

#include <memory>

namespace cynamoncode::testing {

inline QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

inline bool writeBytes(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

// Edycja jak przez użytkownika (historia cofania, isModified).
inline void edit(EditorWidget *editor, const QString &text)
{
    editor->selectAll();
    editor->replaceSelectedText(text);
}

inline void trigger(MainWindow &window, const char *actionName)
{
    auto *action = window.findChild<QAction *>(QLatin1String(actionName));
    QVERIFY2(action, actionName);
    QVERIFY2(action->isEnabled(), actionName);
    action->trigger();
}

// Okno ze skryptowanymi dialogami; opcjonalnie z plikiem ustawień (sesja).
struct WindowFixture
{
    ScriptedPrompts *prompts = nullptr;
    std::unique_ptr<MainWindow> window;

    explicit WindowFixture(const QString &settingsFile = QString())
    {
        auto scripted = std::make_unique<ScriptedPrompts>();
        prompts = scripted.get();
        window = std::make_unique<MainWindow>(
            std::move(scripted), settingsFile.isEmpty()
                                     ? nullptr
                                     : std::make_unique<QSettings>(settingsFile,
                                                                   QSettings::IniFormat));
    }
};

} // namespace cynamoncode::testing
