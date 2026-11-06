#pragma once

// Skryptowana implementacja UserPrompts dla testów GUI (D018): zamiast okien
// modalnych zwraca przygotowane odpowiedzi i zlicza pytania.

#include "ui/UserPrompts.h"

#include <QFont>
#include <QList>
#include <QStringList>

#include <functional>
#include <optional>
#include <utility>

namespace cynamoncode::testing {

class ScriptedPrompts final : public UserPrompts
{
public:
    QList<UnsavedChoice> unsavedAnswers; // brak odpowiedzi = Anuluj
    QStringList savePaths;               // brak ścieżki = anulowanie dialogu
    QStringList openPaths;
    std::optional<int> lineNumber;
    MixedLineEndingChoice mixedAnswer = MixedLineEndingChoice::Keep;
    std::optional<QFont> fontAnswer;
    QList<ExternalChangeChoice> externalAnswers; // brak odpowiedzi = Zostaw
    QList<SaveConflictChoice> conflictAnswers;   // brak odpowiedzi = Anuluj
    // Wywoływane „w trakcie” pytania o zmianę na dysku — jak zagnieżdżona pętla
    // zdarzeń prawdziwego dialogu (np. qWait, zmiany pliku).
    std::function<void()> duringExternalQuestion;
    QStringList externalNames;

    int unsavedQuestions = 0;
    int mixedQuestions = 0;
    int externalQuestions = 0;
    int conflictQuestions = 0;
    bool lastExternalHadLocalChanges = false;
    QString lastSuggestedPath;
    QStringList errors;
    QStringList infos;

    UnsavedChoice askUnsavedChanges(QWidget *, const QString &) override
    {
        ++unsavedQuestions;
        return unsavedAnswers.isEmpty() ? UnsavedChoice::Cancel : unsavedAnswers.takeFirst();
    }
    QStringList askOpenPaths(QWidget *) override { return std::exchange(openPaths, {}); }
    QString askSavePath(QWidget *, const QString &suggestedPath) override
    {
        lastSuggestedPath = suggestedPath;
        return savePaths.isEmpty() ? QString() : savePaths.takeFirst();
    }
    std::optional<int> askLineNumber(QWidget *, int, int) override { return lineNumber; }
    MixedLineEndingChoice askMixedLineEndings(QWidget *, const QString &) override
    {
        ++mixedQuestions;
        return mixedAnswer;
    }
    void showError(QWidget *, const QString &, const QString &message) override
    {
        errors << message;
    }
    std::optional<QFont> askFont(QWidget *, const QFont &) override { return fontAnswer; }
    ExternalChangeChoice askExternalChange(QWidget *, const QString &name,
                                           bool hasLocalChanges) override
    {
        ++externalQuestions;
        externalNames << name;
        lastExternalHadLocalChanges = hasLocalChanges;
        if (duringExternalQuestion)
            std::exchange(duringExternalQuestion, {})();
        return externalAnswers.isEmpty() ? ExternalChangeChoice::Keep
                                         : externalAnswers.takeFirst();
    }
    SaveConflictChoice askSaveConflict(QWidget *, const QString &) override
    {
        ++conflictQuestions;
        return conflictAnswers.isEmpty() ? SaveConflictChoice::Cancel
                                         : conflictAnswers.takeFirst();
    }
    void showInfo(QWidget *, const QString &, const QString &message) override
    {
        infos << message;
    }
};

} // namespace cynamoncode::testing
