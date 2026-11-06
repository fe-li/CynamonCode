// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/ShortcutsDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>

namespace cynamoncode {

namespace {

enum Column { CategoryColumn, TitleColumn, ShortcutColumn, ColumnCount };

} // namespace

ShortcutsDialog::ShortcutsDialog(QList<ShortcutEntry> entries, QWidget *parent)
    : QDialog(parent)
    , m_entries(std::move(entries))
{
    setObjectName(QStringLiteral("shortcutsDialog"));
    setWindowTitle(tr("Skróty klawiszowe"));

    auto *filterLabel = new QLabel(tr("&Szukaj polecenia:"), this);
    m_filter = new QLineEdit(this);
    m_filter->setObjectName(QStringLiteral("filterEdit"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setPlaceholderText(tr("nazwa polecenia, menu lub skrót"));
    filterLabel->setBuddy(m_filter);
    connect(m_filter, &QLineEdit::textChanged, this, [this] { applyFilter(); });

    m_table = new QTableWidget(static_cast<int>(m_entries.size()), ColumnCount, this);
    m_table->setObjectName(QStringLiteral("shortcutTable"));
    m_table->setAccessibleName(tr("Lista poleceń i skrótów"));
    m_table->setHorizontalHeaderLabels({tr("Menu"), tr("Polecenie"), tr("Skrót")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setTabKeyNavigation(false); // Tab przechodzi do kolejnych kontrolek
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(TitleColumn, QHeaderView::Stretch);
    for (int row = 0; row < m_entries.size(); ++row) {
        for (int column = 0; column < ColumnCount; ++column)
            m_table->setItem(row, column, new QTableWidgetItem);
        m_table->item(row, CategoryColumn)->setData(Qt::UserRole, m_entries.at(row).id);
        refreshRow(row);
    }
    m_table->resizeColumnToContents(CategoryColumn);
    m_table->resizeColumnToContents(ShortcutColumn);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const int index = indexOf(selectedId());
        m_sequenceEdit->setKeySequence(index >= 0 && !m_entries.at(index).current.isEmpty()
                                           ? m_entries.at(index).current.constFirst()
                                           : QKeySequence());
        updateButtons();
    });

    auto *sequenceLabel = new QLabel(tr("&Nowy skrót:"), this);
    m_sequenceEdit = new QKeySequenceEdit(this);
    m_sequenceEdit->setObjectName(QStringLiteral("sequenceEdit"));
    m_sequenceEdit->setAccessibleName(tr("Nowy skrót — naciśnij kombinację klawiszy"));
    sequenceLabel->setBuddy(m_sequenceEdit);
    connect(m_sequenceEdit, &QKeySequenceEdit::keySequenceChanged, this, [this] {
        // Tylko jedna kombinacja (Qt 6.4 nie ma setMaximumSequenceLength): kolejne
        // klawisze, np. Tab naciśnięty, by przejść dalej, nie dopisują się do skrótu.
        const QKeySequence sequence = m_sequenceEdit->keySequence();
        if (sequence.count() > 1) {
            m_sequenceEdit->setKeySequence(QKeySequence(sequence[0]));
            m_assignButton->setFocus();
        }
        updateButtons();
    });
    // Po nagraniu skrótu fokus przechodzi do „Przypisz” — obsługa bez myszy.
    connect(m_sequenceEdit, &QKeySequenceEdit::editingFinished, this, [this] {
        if (m_assignButton->isEnabled())
            m_assignButton->setFocus();
    });

    m_assignButton = new QPushButton(tr("&Przypisz"), this);
    m_assignButton->setObjectName(QStringLiteral("assignButton"));
    connect(m_assignButton, &QPushButton::clicked, this,
            [this] { assign(selectedId(), m_sequenceEdit->keySequence()); });

    m_clearButton = new QPushButton(tr("&Usuń skrót"), this);
    m_clearButton->setObjectName(QStringLiteral("clearButton"));
    connect(m_clearButton, &QPushButton::clicked, this, [this] { clear(selectedId()); });

    m_resetButton = new QPushButton(tr("Przywróć &domyślny"), this);
    m_resetButton->setObjectName(QStringLiteral("resetButton"));
    connect(m_resetButton, &QPushButton::clicked, this, [this] { resetToDefault(selectedId()); });

    auto *resetAllButton = new QPushButton(tr("Przywróć &wszystkie domyślne"), this);
    resetAllButton->setObjectName(QStringLiteral("resetAllButton"));
    connect(resetAllButton, &QPushButton::clicked, this, [this] { resetAll(); });

    m_message = new QLabel(this);
    m_message->setObjectName(QStringLiteral("messageLabel"));
    m_message->setWordWrap(true);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Anuluj"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    // Enter w polu wyszukiwania nie zatwierdza całego okna.
    for (QAbstractButton *button : buttons->buttons()) {
        if (auto *push = qobject_cast<QPushButton *>(button)) {
            push->setAutoDefault(false);
            push->setDefault(false);
        }
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(filterLabel);
    filterRow->addWidget(m_filter, 1);

    auto *editRow = new QHBoxLayout;
    editRow->addWidget(sequenceLabel);
    editRow->addWidget(m_sequenceEdit, 1);
    editRow->addWidget(m_assignButton);
    editRow->addWidget(m_clearButton);
    editRow->addWidget(m_resetButton);

    auto *hint = new QLabel(tr("Skrót musi zawierać Ctrl, Alt lub Super albo być klawiszem "
                               "F1–F35. Zmiany obowiązują po kliknięciu „OK”."),
                            this);
    hint->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(filterRow);
    layout->addWidget(m_table, 1);
    layout->addLayout(editRow);
    layout->addWidget(hint);
    layout->addWidget(m_message);
    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(resetAllButton);
    bottomRow->addStretch();
    bottomRow->addWidget(buttons);
    layout->addLayout(bottomRow);

    resize(720, 560);
    if (!m_entries.isEmpty())
        m_table->selectRow(0);
    updateButtons();
}

QString ShortcutsDialog::message() const
{
    return m_message->text();
}

int ShortcutsDialog::indexOf(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id)
            return i;
    }
    return -1;
}

QStringList ShortcutsDialog::takeFromOthers(const QKeySequence &sequence, int except)
{
    QStringList notes;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (i == except || !m_entries[i].current.removeAll(sequence))
            continue;
        refreshRow(i);
        notes.append(tr("Skrót %1 zabrano poleceniu „%2” (%3).")
                         .arg(sequence.toString(QKeySequence::NativeText),
                              m_entries.at(i).title, m_entries.at(i).category));
    }
    return notes;
}

bool ShortcutsDialog::assign(const QString &id, const QKeySequence &recorded)
{
    const int index = indexOf(id);
    if (index < 0)
        return false;
    const QKeySequence sequence = Shortcuts::normalized(recorded);
    if (!Shortcuts::isAllowed(sequence)) {
        setMessage(tr("Nie przypisano: skrót to jedna kombinacja klawiszy z Ctrl, Alt lub "
                      "Super albo klawisz F1–F35."));
        return false;
    }
    if (m_entries.at(index).current.contains(sequence)) {
        // Bez zmian — także drugi skrót polecenia (np. Ctrl+PgDown) zostaje.
        setMessage(tr("Polecenie „%1” ma już skrót %2.")
                       .arg(m_entries.at(index).title,
                            sequence.toString(QKeySequence::NativeText)));
        return false;
    }
    QStringList notes = takeFromOthers(sequence, index);
    m_entries[index].current = {sequence};
    refreshRow(index);
    notes.prepend(tr("Polecenie „%1” ma skrót %2.")
                      .arg(m_entries.at(index).title,
                           sequence.toString(QKeySequence::NativeText)));
    setMessage(notes.join(QLatin1Char(' ')));
    updateButtons();
    return true;
}

bool ShortcutsDialog::clear(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || m_entries.at(index).current.isEmpty())
        return false;
    m_entries[index].current.clear();
    refreshRow(index);
    m_sequenceEdit->clear();
    setMessage(tr("Polecenie „%1” nie ma skrótu.").arg(m_entries.at(index).title));
    updateButtons();
    return true;
}

bool ShortcutsDialog::resetToDefault(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || m_entries.at(index).current == m_entries.at(index).defaults)
        return false;
    QStringList notes;
    for (const QKeySequence &sequence : m_entries.at(index).defaults)
        notes += takeFromOthers(sequence, index);
    m_entries[index].current = m_entries.at(index).defaults;
    refreshRow(index);
    notes.prepend(tr("Przywrócono domyślny skrót polecenia „%1”.").arg(m_entries.at(index).title));
    setMessage(notes.join(QLatin1Char(' ')));
    updateButtons();
    return true;
}

void ShortcutsDialog::resetAll()
{
    for (int i = 0; i < m_entries.size(); ++i) {
        m_entries[i].current = m_entries.at(i).defaults;
        refreshRow(i);
    }
    setMessage(tr("Przywrócono wszystkie domyślne skróty."));
    updateButtons();
}

void ShortcutsDialog::setFilter(const QString &text)
{
    m_filter->setText(text);
}

bool ShortcutsDialog::selectCommand(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || m_table->isRowHidden(index))
        return false;
    m_table->selectRow(index);
    return true;
}

QString ShortcutsDialog::selectedId() const
{
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty())
        return {};
    return m_table->item(selected.constFirst()->row(), CategoryColumn)
        ->data(Qt::UserRole)
        .toString();
}

void ShortcutsDialog::refreshRow(int index)
{
    const ShortcutEntry &entry = m_entries.at(index);
    m_table->item(index, CategoryColumn)->setText(entry.category);
    m_table->item(index, TitleColumn)->setText(entry.title);
    QString shortcut = Shortcuts::displayText(entry.current);
    if (entry.current != entry.defaults)
        shortcut += tr(" (zmieniony)");
    m_table->item(index, ShortcutColumn)->setText(shortcut);
    QFont font = m_table->item(index, ShortcutColumn)->font();
    font.setBold(entry.current != entry.defaults);
    m_table->item(index, ShortcutColumn)->setFont(font);
}

void ShortcutsDialog::applyFilter()
{
    const QString needle = m_filter->text().trimmed();
    for (int row = 0; row < m_entries.size(); ++row) {
        const ShortcutEntry &entry = m_entries.at(row);
        const bool matches =
            needle.isEmpty() || entry.title.contains(needle, Qt::CaseInsensitive)
            || entry.category.contains(needle, Qt::CaseInsensitive)
            || Shortcuts::displayText(entry.current).contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(row, !matches);
    }
    const int selected = indexOf(selectedId());
    if (selected >= 0 && m_table->isRowHidden(selected))
        m_table->clearSelection();
    updateButtons();
}

void ShortcutsDialog::updateButtons()
{
    const int index = indexOf(selectedId());
    const bool hasCommand = index >= 0;
    m_sequenceEdit->setEnabled(hasCommand);
    m_assignButton->setEnabled(hasCommand && !m_sequenceEdit->keySequence().isEmpty());
    m_clearButton->setEnabled(hasCommand && !m_entries.at(index).current.isEmpty());
    m_resetButton->setEnabled(hasCommand
                              && m_entries.at(index).current != m_entries.at(index).defaults);
}

void ShortcutsDialog::setMessage(const QString &text)
{
    m_message->setText(text);
}

} // namespace cynamoncode
