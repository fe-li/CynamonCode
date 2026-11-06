// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QCoreApplication>
#include <QFont>
#include <QString>
#include <QStringList>

#include <optional>

class QWidget;

namespace cynamoncode {

enum class UnsavedChoice {
    Save,
    Discard,
    Cancel,
};

enum class MixedLineEndingChoice {
    Keep, // domyślne (D020)
    ConvertToLf,
    ConvertToCrLf,
};

enum class ExternalChangeChoice {
    Reload,
    Keep,
};

enum class SaveConflictChoice {
    Overwrite,
    SaveAs,
    Cancel,
};

// Wszystkie pytania i komunikaty okna głównego. Domyślnie DialogPrompts
// (modalne dialogi Qt); testy podstawiają implementację skryptowaną (D018).
class UserPrompts
{
public:
    virtual ~UserPrompts() = default;

    virtual UnsavedChoice askUnsavedChanges(QWidget *parent, const QString &documentName) = 0;
    virtual QStringList askOpenPaths(QWidget *parent) = 0;
    // Pusty wynik oznacza anulowanie.
    virtual QString askSavePath(QWidget *parent, const QString &suggestedPath) = 0;
    virtual std::optional<int> askLineNumber(QWidget *parent, int currentLine, int lineCount) = 0;
    virtual MixedLineEndingChoice askMixedLineEndings(QWidget *parent,
                                                      const QString &documentName) = 0;
    virtual void showError(QWidget *parent, const QString &title, const QString &message) = 0;
    // Wybór czcionki edytora; nullopt = anulowano.
    virtual std::optional<QFont> askFont(QWidget *parent, const QFont &current) = 0;
    // Plik zmieniony przez inny program (D026).
    virtual ExternalChangeChoice askExternalChange(QWidget *parent, const QString &documentName,
                                                   bool hasLocalChanges) = 0;
    // Zapis przy zmienionym pliku na dysku (D027); `path` — pełna ścieżka.
    virtual SaveConflictChoice askSaveConflict(QWidget *parent, const QString &path) = 0;
    virtual void showInfo(QWidget *parent, const QString &title, const QString &message) = 0;
    // Folder dla panelu plików (D037); pusty = anulowano.
    virtual QString askFolder(QWidget *parent, const QString &current) = 0;
    // Plik powyżej progu ostrożności (D039).
    virtual bool askOpenLargeFile(QWidget *parent, const QString &path, qint64 bytes) = 0;
    // Potwierdzenie „Zamień we wszystkich otwartych” (D038).
    virtual bool askReplaceInAll(QWidget *parent, int matches, int documents) = 0;
};

class DialogPrompts final : public UserPrompts
{
    Q_DECLARE_TR_FUNCTIONS(DialogPrompts)

public:
    UnsavedChoice askUnsavedChanges(QWidget *parent, const QString &documentName) override;
    QStringList askOpenPaths(QWidget *parent) override;
    QString askSavePath(QWidget *parent, const QString &suggestedPath) override;
    std::optional<int> askLineNumber(QWidget *parent, int currentLine, int lineCount) override;
    MixedLineEndingChoice askMixedLineEndings(QWidget *parent,
                                              const QString &documentName) override;
    void showError(QWidget *parent, const QString &title, const QString &message) override;
    std::optional<QFont> askFont(QWidget *parent, const QFont &current) override;
    ExternalChangeChoice askExternalChange(QWidget *parent, const QString &documentName,
                                           bool hasLocalChanges) override;
    SaveConflictChoice askSaveConflict(QWidget *parent, const QString &path) override;
    void showInfo(QWidget *parent, const QString &title, const QString &message) override;
    QString askFolder(QWidget *parent, const QString &current) override;
    bool askOpenLargeFile(QWidget *parent, const QString &path, qint64 bytes) override;
    bool askReplaceInAll(QWidget *parent, int matches, int documents) override;
};

} // namespace cynamoncode
