// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/FindReplaceDialog.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace cynamoncode {

FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("findReplaceDialog"));
    setWindowTitle(tr("Znajdź i zamień"));
    setModal(false);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setObjectName(QStringLiteral("findEdit"));
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setObjectName(QStringLiteral("replaceEdit"));

    auto *fields = new QFormLayout;
    fields->addRow(tr("&Znajdź:"), m_findEdit);
    fields->addRow(tr("Za&mień na:"), m_replaceEdit);

    m_caseCheck = new QCheckBox(tr("Uwzględniaj &wielkość liter"), this);
    m_caseCheck->setObjectName(QStringLiteral("caseCheck"));
    m_wordsCheck = new QCheckBox(tr("Tylko &całe wyrazy"), this);
    m_wordsCheck->setObjectName(QStringLiteral("wordsCheck"));
    m_regexCheck = new QCheckBox(tr("Wyrażenie &regularne"), this);
    m_regexCheck->setObjectName(QStringLiteral("regexCheck"));
    m_wrapCheck = new QCheckBox(tr("&Zawijaj wyszukiwanie"), this);
    m_wrapCheck->setObjectName(QStringLiteral("wrapCheck"));
    m_wrapCheck->setChecked(true);

    auto *checks = new QVBoxLayout;
    for (QCheckBox *check : {m_caseCheck, m_wordsCheck, m_regexCheck, m_wrapCheck})
        checks->addWidget(check);

    const auto makeButton = [this](const QString &text, const char *name) {
        auto *button = new QPushButton(text, this);
        button->setObjectName(QLatin1String(name));
        button->setAutoDefault(false);
        return button;
    };
    QPushButton *findNext = makeButton(tr("Znajdź &następny"), "findNextButton");
    QPushButton *findPrevious = makeButton(tr("Znajdź &poprzedni"), "findPreviousButton");
    QPushButton *replace = makeButton(tr("Za&mień"), "replaceButton");
    QPushButton *replaceAll = makeButton(tr("Zamień &wszystkie"), "replaceAllButton");
    QPushButton *findInAll = makeButton(tr("Znajdź we wszystkich &otwartych"), "findInAllButton");
    QPushButton *replaceInAll =
        makeButton(tr("Zamień we wszystkich o&twartych"), "replaceInAllButton");
    QPushButton *close = makeButton(tr("Zamknij"), "closeButton");

    auto *buttons = new QVBoxLayout;
    for (QPushButton *button : {findNext, findPrevious, replace, replaceAll})
        buttons->addWidget(button);
    buttons->addSpacing(8);
    buttons->addWidget(findInAll);
    buttons->addWidget(replaceInAll);
    buttons->addSpacing(8);
    buttons->addWidget(close);
    buttons->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setWordWrap(true);

    auto *left = new QVBoxLayout;
    left->addLayout(fields);
    left->addLayout(checks);
    left->addWidget(m_statusLabel);
    left->addStretch();

    auto *layout = new QHBoxLayout(this);
    layout->addLayout(left, 1);
    layout->addLayout(buttons);

    connect(findNext, &QPushButton::clicked, this, [this] { emit findRequested(true); });
    connect(findPrevious, &QPushButton::clicked, this, [this] { emit findRequested(false); });
    connect(replace, &QPushButton::clicked, this, &FindReplaceDialog::replaceRequested);
    connect(replaceAll, &QPushButton::clicked, this, &FindReplaceDialog::replaceAllRequested);
    connect(findInAll, &QPushButton::clicked, this, &FindReplaceDialog::findInAllRequested);
    connect(replaceInAll, &QPushButton::clicked, this, &FindReplaceDialog::replaceInAllRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::hide);
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { emit findRequested(true); });
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindReplaceDialog::replaceRequested);
    // Zmiana wzorca lub opcji czyści poprzedni komunikat.
    for (QCheckBox *check : {m_caseCheck, m_wordsCheck, m_regexCheck, m_wrapCheck})
        connect(check, &QCheckBox::toggled, m_statusLabel, &QLabel::clear);
    connect(m_findEdit, &QLineEdit::textChanged, m_statusLabel, &QLabel::clear);
}

SearchOptions FindReplaceDialog::options() const
{
    SearchOptions options;
    options.pattern = m_findEdit->text();
    options.caseSensitive = m_caseCheck->isChecked();
    options.wholeWords = m_wordsCheck->isChecked();
    options.regex = m_regexCheck->isChecked();
    options.wrap = m_wrapCheck->isChecked();
    return options;
}

QString FindReplaceDialog::replacement() const
{
    return m_replaceEdit->text();
}

bool FindReplaceDialog::hasPattern() const
{
    return !m_findEdit->text().isEmpty();
}

void FindReplaceDialog::setPattern(const QString &pattern)
{
    m_findEdit->setText(pattern);
}

void FindReplaceDialog::setStatus(const QString &message)
{
    m_statusLabel->setText(message);
}

void FindReplaceDialog::showForFind()
{
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceDialog::showForReplace()
{
    showForFind();
    if (!m_findEdit->text().isEmpty())
        m_replaceEdit->setFocus();
}

} // namespace cynamoncode
