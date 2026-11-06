// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/Shortcuts.h"

namespace cynamoncode::Shortcuts {

bool isAllowed(const QKeySequence &sequence)
{
    if (sequence.count() != 1)
        return false;
    const QKeyCombination first = sequence[0];
    const Qt::Key key = first.key();
    if (key == Qt::Key_unknown || key == 0 || key == Qt::Key_Control || key == Qt::Key_Shift
        || key == Qt::Key_Alt || key == Qt::Key_Meta || key == Qt::Key_AltGr)
        return false;
    const Qt::KeyboardModifiers modifiers = first.keyboardModifiers();
    if (modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return true;
    return first.key() >= Qt::Key_F1 && first.key() <= Qt::Key_F35;
}

QKeySequence normalized(const QKeySequence &sequence)
{
    if (sequence.count() != 1 || sequence[0].key() != Qt::Key_Backtab)
        return sequence;
    return QKeySequence(QKeyCombination(sequence[0].keyboardModifiers() | Qt::ShiftModifier,
                                        Qt::Key_Tab));
}

QStringList toStorage(const QList<QKeySequence> &sequences)
{
    QStringList result;
    for (const QKeySequence &sequence : sequences)
        result.append(sequence.toString(QKeySequence::PortableText));
    return result;
}

QList<QKeySequence> fromStorage(const QStringList &texts)
{
    QList<QKeySequence> result;
    for (const QString &text : texts) {
        const QKeySequence sequence =
            normalized(QKeySequence::fromString(text, QKeySequence::PortableText));
        if (isAllowed(sequence) && !result.contains(sequence))
            result.append(sequence);
    }
    return result;
}

QString displayText(const QList<QKeySequence> &sequences)
{
    QStringList parts;
    for (const QKeySequence &sequence : sequences)
        parts.append(sequence.toString(QKeySequence::NativeText));
    return parts.join(QStringLiteral(", "));
}

QString plainText(const QString &menuText)
{
    QString result;
    result.reserve(menuText.size());
    for (qsizetype i = 0; i < menuText.size(); ++i) {
        if (menuText.at(i) == QLatin1Char('&')) {
            if (i + 1 < menuText.size() && menuText.at(i + 1) == QLatin1Char('&')) {
                result.append(QLatin1Char('&'));
                ++i;
            }
            continue;
        }
        result.append(menuText.at(i));
    }
    if (result.endsWith(QChar(0x2026)))
        result.chop(1);
    return result.trimmed();
}

} // namespace cynamoncode::Shortcuts
