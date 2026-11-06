#pragma once

#include <QCoreApplication>
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
    virtual void showError(QWidget *parent, const QString &title, const QString &message) = 0;
};

class DialogPrompts final : public UserPrompts
{
    Q_DECLARE_TR_FUNCTIONS(DialogPrompts)

public:
    UnsavedChoice askUnsavedChanges(QWidget *parent, const QString &documentName) override;
    QStringList askOpenPaths(QWidget *parent) override;
    QString askSavePath(QWidget *parent, const QString &suggestedPath) override;
    std::optional<int> askLineNumber(QWidget *parent, int currentLine, int lineCount) override;
    void showError(QWidget *parent, const QString &title, const QString &message) override;
};

} // namespace cynamoncode
