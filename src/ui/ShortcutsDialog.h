// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "ui/Shortcuts.h"

#include <QDialog>
#include <QList>

class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace cynamoncode {

// Ustawienia → „Skróty klawiszowe…” (D045). Pracuje na kopii listy poleceń;
// okno główne stosuje wynik dopiero po „OK”. Skrót zajęty przez inne polecenie
// jest przenoszony, a okno informuje, komu go zabrano.
class ShortcutsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutsDialog(QList<ShortcutEntry> entries, QWidget *parent = nullptr);

    const QList<ShortcutEntry> &entries() const { return m_entries; }
    QString message() const;

    // Operacje także dla testów; zwracają false, gdy nic nie zmieniono.
    bool assign(const QString &id, const QKeySequence &recorded);
    bool clear(const QString &id);
    bool resetToDefault(const QString &id);
    void resetAll();
    void setFilter(const QString &text);
    bool selectCommand(const QString &id);
    QString selectedId() const;

private:
    int indexOf(const QString &id) const;
    // Zabiera `sequence` innym poleceniom niż `except`; zwraca opis zmian.
    QStringList takeFromOthers(const QKeySequence &sequence, int except);
    void refreshRow(int index);
    void applyFilter();
    void updateButtons();
    void setMessage(const QString &text);

    QList<ShortcutEntry> m_entries;
    QLineEdit *m_filter = nullptr;
    QTableWidget *m_table = nullptr;
    QKeySequenceEdit *m_sequenceEdit = nullptr;
    QPushButton *m_assignButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QLabel *m_message = nullptr;
};

} // namespace cynamoncode
