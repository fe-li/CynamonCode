// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

// Konfigurowalne skróty klawiszowe (D045): opis polecenia i reguły skrótów.
namespace cynamoncode {

struct ShortcutEntry
{
    QString id;       // nazwa obiektu QAction, np. "actionNew" — klucz w ustawieniach
    QString category; // ścieżka menu bez znaczników „&”, np. "Widok → Motyw"
    QString title;    // nazwa polecenia bez „&” i „…”
    QList<QKeySequence> defaults;
    QList<QKeySequence> current;
};

namespace Shortcuts {

// Dozwolony skrót: dokładnie jedna kombinacja klawiszy z Ctrl, Alt lub Super albo
// klawisz F1–F35. Zwykłe litery, Tab, Enter i Esc zostają dla edycji tekstu;
// sekwencje kilku kombinacji (np. „Ctrl+K, Ctrl+C”) nie są obsługiwane — ich
// pierwsza część zasłaniałaby inne skróty i połykała następny klawisz.
bool isAllowed(const QKeySequence &sequence);

// Postać kanoniczna: Shift+Backtab (tak X11 zgłasza Shift+Tab) → Shift+Tab.
QKeySequence normalized(const QKeySequence &sequence);

// Format zapisu w ustawieniach: QKeySequence::PortableText.
QStringList toStorage(const QList<QKeySequence> &sequences);
// Pomija wpisy błędne, puste i niedozwolone.
QList<QKeySequence> fromStorage(const QStringList &texts);

// Tekst dla użytkownika, np. "Ctrl+Y, Ctrl+Shift+Z".
QString displayText(const QList<QKeySequence> &sequences);

// Tekst menu bez znaczników „&” (z „&&” → „&”) i bez końcowego „…”.
QString plainText(const QString &menuText);

} // namespace Shortcuts
} // namespace cynamoncode
