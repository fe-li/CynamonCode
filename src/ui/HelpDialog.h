// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include "ui/Shortcuts.h"

#include <QDialog>
#include <QList>

class QTextBrowser;

namespace cynamoncode {

// Pomoc → „Pomoc programu” (F1, D046): opis funkcji, zasady pracy z plikami
// i aktualna lista skrótów. Treść lokalna, bez sieci.
class HelpDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

    // Odświeża treść (lista skrótów może się zmienić w ustawieniach).
    void setShortcuts(const QList<ShortcutEntry> &entries);
    QString html() const;

private:
    QTextBrowser *m_browser = nullptr;
};

} // namespace cynamoncode
