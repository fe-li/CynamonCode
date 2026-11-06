#include "ui/UserPrompts.h"

#include <QAbstractButton>
#include <QFileDialog>
#include <QFontDialog>
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

ExternalChangeChoice DialogPrompts::askExternalChange(QWidget *parent, const QString &documentName,
                                                     bool hasLocalChanges)
{
    QMessageBox box(parent);
    box.setIcon(hasLocalChanges ? QMessageBox::Warning : QMessageBox::Question);
    box.setWindowTitle(tr("Plik zmieniony na dysku"));
    QPushButton *reload = nullptr;
    QPushButton *keep = nullptr;
    if (hasLocalChanges) {
        box.setText(tr("Plik „%1” został zmieniony przez inny program, a dokument ma "
                       "niezapisane zmiany.")
                        .arg(documentName));
        box.setInformativeText(tr("Przeładowanie porzuci Twoje zmiany. Zachowanie ich spowoduje "
                                  "pytanie przed nadpisaniem pliku przy zapisie."));
        reload = box.addButton(tr("Przeładuj i porzuć moje zmiany"), QMessageBox::DestructiveRole);
        keep = box.addButton(tr("Zachowaj moje zmiany"), QMessageBox::RejectRole);
    } else {
        box.setText(tr("Plik „%1” został zmieniony przez inny program.").arg(documentName));
        box.setInformativeText(tr("Przeładować go z dysku?"));
        reload = box.addButton(tr("Przeładuj"), QMessageBox::AcceptRole);
        keep = box.addButton(tr("Zostaw"), QMessageBox::RejectRole);
    }
    box.setDefaultButton(keep);
    box.setEscapeButton(keep);
    box.exec();
    return box.clickedButton() == reload ? ExternalChangeChoice::Reload : ExternalChangeChoice::Keep;
}

SaveConflictChoice DialogPrompts::askSaveConflict(QWidget *parent, const QString &path)
{
    QMessageBox box(QMessageBox::Warning, tr("Konflikt zapisu"),
                    tr("Plik na dysku zmienił się od ostatniego wczytania lub zapisu:\n%1")
                        .arg(path),
                    QMessageBox::NoButton, parent);
    box.setInformativeText(tr("Nadpisanie usunie zmiany wprowadzone przez inny program."));
    QPushButton *overwrite = box.addButton(tr("Nadpisz"), QMessageBox::DestructiveRole);
    QPushButton *saveAs = box.addButton(tr("Zapisz jako…"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(tr("Anuluj"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() == overwrite)
        return SaveConflictChoice::Overwrite;
    if (box.clickedButton() == saveAs)
        return SaveConflictChoice::SaveAs;
    return SaveConflictChoice::Cancel;
}

void DialogPrompts::showInfo(QWidget *parent, const QString &title, const QString &message)
{
    QMessageBox::information(parent, title, message);
}

QString DialogPrompts::askFolder(QWidget *parent, const QString &current)
{
    return QFileDialog::getExistingDirectory(parent, tr("Otwórz folder"), current);
}

bool DialogPrompts::askOpenLargeFile(QWidget *parent, const QString &path, qint64 bytes)
{
    QMessageBox box(QMessageBox::Warning, tr("Duży plik"),
                    tr("Plik %1 ma %2 MiB.\n\nBardzo duże pliki mogą otwierać się długo i "
                       "spowalniać edytor.")
                        .arg(path)
                        .arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1),
                    QMessageBox::NoButton, parent);
    QPushButton *open = box.addButton(tr("Otwórz mimo to"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(tr("Anuluj"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == open;
}

bool DialogPrompts::askReplaceInAll(QWidget *parent, int matches, int documents)
{
    QMessageBox box(QMessageBox::Question, tr("Zamień we wszystkich otwartych"),
                    tr("Zamienić %1 wystąpień w %2 dokumentach?\n\nPliki na dysku zmienią się "
                       "dopiero po zapisaniu; każdy dokument można cofnąć osobno (Ctrl+Z).")
                        .arg(matches)
                        .arg(documents),
                    QMessageBox::NoButton, parent);
    QPushButton *replace = box.addButton(tr("Zamień"), QMessageBox::AcceptRole);
    QPushButton *cancel = box.addButton(tr("Anuluj"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == replace;
}

std::optional<QFont> DialogPrompts::askFont(QWidget *parent, const QFont &current)
{
    bool accepted = false;
    const QFont font = QFontDialog::getFont(&accepted, current, parent, tr("Czcionka edytora"),
                                            QFontDialog::MonospacedFonts);
    if (!accepted)
        return std::nullopt;
    return font;
}

} // namespace cynamoncode
