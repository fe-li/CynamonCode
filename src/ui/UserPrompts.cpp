#include "ui/UserPrompts.h"

#include <QAbstractButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

namespace cynamoncode {

UnsavedChoice DialogPrompts::askUnsavedChanges(QWidget *parent, const QString &documentName)
{
    QMessageBox box(QMessageBox::Warning, tr("Niezapisane zmiany"),
                    tr("Dokument „%1” ma niezapisane zmiany.\nCzy zapisać je przed zamknięciem?")
                        .arg(documentName),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, parent);
    box.button(QMessageBox::Save)->setText(tr("Zapisz"));
    box.button(QMessageBox::Discard)->setText(tr("Nie zapisuj"));
    box.button(QMessageBox::Cancel)->setText(tr("Anuluj"));
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);

    switch (box.exec()) {
    case QMessageBox::Save:
        return UnsavedChoice::Save;
    case QMessageBox::Discard:
        return UnsavedChoice::Discard;
    default:
        return UnsavedChoice::Cancel;
    }
}

QStringList DialogPrompts::askOpenPaths(QWidget *parent)
{
    return QFileDialog::getOpenFileNames(parent, tr("Otwórz plik"), QString(),
                                         tr("Wszystkie pliki (*)"));
}

QString DialogPrompts::askSavePath(QWidget *parent, const QString &suggestedPath)
{
    // QFileDialog sam pyta o nadpisanie istniejącego pliku.
    return QFileDialog::getSaveFileName(parent, tr("Zapisz jako"), suggestedPath,
                                        tr("Wszystkie pliki (*)"));
}

std::optional<int> DialogPrompts::askLineNumber(QWidget *parent, int currentLine, int lineCount)
{
    bool accepted = false;
    const int line = QInputDialog::getInt(parent, tr("Przejdź do wiersza"),
                                          tr("Numer wiersza (1–%1):").arg(lineCount), currentLine,
                                          1, lineCount, 1, &accepted);
    if (!accepted)
        return std::nullopt;
    return line;
}

MixedLineEndingChoice DialogPrompts::askMixedLineEndings(QWidget *parent,
                                                        const QString &documentName)
{
    QMessageBox box(QMessageBox::Information, tr("Mieszane końce linii"),
                    tr("Plik „%1” zawiera różne końce linii (LF i CRLF).\n\n"
                       "Ujednolicenie oznaczy dokument jako zmieniony — plik na dysku "
                       "zmieni się dopiero po zapisaniu.")
                        .arg(documentName),
                    QMessageBox::NoButton, parent);
    QPushButton *keep = box.addButton(tr("Zachowaj bez zmian"), QMessageBox::RejectRole);
    QPushButton *toLf = box.addButton(tr("Ujednolić do LF"), QMessageBox::AcceptRole);
    QPushButton *toCrLf = box.addButton(tr("Ujednolić do CRLF"), QMessageBox::AcceptRole);
    box.setDefaultButton(keep);
    box.setEscapeButton(keep);
    box.exec();

    if (box.clickedButton() == toLf)
        return MixedLineEndingChoice::ConvertToLf;
    if (box.clickedButton() == toCrLf)
        return MixedLineEndingChoice::ConvertToCrLf;
    return MixedLineEndingChoice::Keep;
}

void DialogPrompts::showError(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::warning(parent, title, message);
}

} // namespace cynamoncode
