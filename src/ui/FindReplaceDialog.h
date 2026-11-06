#pragma once

#include "domain/SearchOptions.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace cynamoncode {

// Niemodalne okno „Znajdź i zamień” (D028). Samo nie szuka — zgłasza żądania,
// które wykonuje MainWindow na bieżącej karcie.
class FindReplaceDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit FindReplaceDialog(QWidget *parent = nullptr);

    SearchOptions options() const;
    QString replacement() const;
    bool hasPattern() const;

    void setPattern(const QString &pattern);
    void setStatus(const QString &message);

    void showForFind();
    void showForReplace();

signals:
    void findRequested(bool forward);
    void replaceRequested();
    void replaceAllRequested();

private:
    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_caseCheck = nullptr;
    QCheckBox *m_wordsCheck = nullptr;
    QCheckBox *m_regexCheck = nullptr;
    QCheckBox *m_wrapCheck = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace cynamoncode
