// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/HelpDialog.h"

#include "app/AppInfo.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace cynamoncode {

namespace {

QString section(const QString &title, const QStringList &items)
{
    QString html = QStringLiteral("<h3>%1</h3><ul>").arg(title.toHtmlEscaped());
    for (const QString &item : items)
        html += QStringLiteral("<li>%1</li>").arg(item.toHtmlEscaped());
    return html + QStringLiteral("</ul>");
}

} // namespace

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("helpDialog"));
    setWindowTitle(tr("Pomoc — %1").arg(AppInfo::productName()));

    m_browser = new QTextBrowser(this);
    m_browser->setObjectName(QStringLiteral("helpBrowser"));
    m_browser->setAccessibleName(tr("Treść pomocy"));
    // Bez sieci i bez otwierania odnośników — pomoc jest w całości lokalna.
    m_browser->setOpenLinks(false);
    m_browser->setOpenExternalLinks(false);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Zamknij"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_browser, 1);
    layout->addWidget(buttons);
    resize(720, 640);
    setShortcuts({});
}

void HelpDialog::setShortcuts(const QList<ShortcutEntry> &entries)
{
    QString html = QStringLiteral("<h2>%1 %2</h2><p>%3</p>")
                       .arg(AppInfo::productName(), AppInfo::version(),
                            tr("Lekki edytor tekstu i kodu z kolorowaniem składni. Program "
                               "celowo nie podpowiada kodu, nie sprawdza jego poprawności i "
                               "nie uruchamia otwartych plików.")
                                .toHtmlEscaped());

    html += section(
        tr("Pliki"),
        {tr("Otwierane są pliki tekstowe w UTF-8 (z BOM lub bez). Pliki w innym kodowaniu "
            "i pliki binarne nie są otwierane — program pokazuje komunikat i niczego nie "
            "zmienia."),
         tr("Zapis jest atomowy: plik na dysku zmienia się dopiero po udanym zapisie całości. "
            "Końce linii (LF, CRLF) i znacznik BOM są zachowywane."),
         tr("Gdy plik zmieni inny program, CynamonCode pyta, czy go przeładować. Zapis pliku "
            "zmienionego na dysku wymaga potwierdzenia."),
         tr("Przed zamknięciem karty lub programu z niezapisanymi zmianami pojawia się pytanie.")});

    html += section(
        tr("Edycja i wygląd"),
        {tr("Język kolorowania jest wykrywany z rozszerzenia, nazwy pliku lub pierwszej linii; "
            "można go zmienić w menu Język."),
         tr("Menu Widok: motyw jasny lub ciemny, zawijanie wierszy, numery wierszy, białe "
            "znaki, powiększenie, panel plików i wyniki wyszukiwania."),
         tr("Dwa widoki: kartę można przenieść albo sklonować do drugiego widoku (menu Widok "
            "lub menu karty). Klon pokazuje ten sam dokument.")});

    html += section(
        tr("Wyszukiwanie"),
        {tr("Znajdź i zamień obsługuje wielkość liter, całe wyrazy i wyrażenia regularne; "
            "„Zamień wszystkie” można cofnąć jednym Ctrl+Z."),
         tr("„Znajdź we wszystkich otwartych” pokazuje wyniki w panelu; kliknięcie wyniku "
            "przechodzi do trafienia.")});

    html += section(
        tr("Ustawienia i sesja"),
        {tr("Ustawienia są zapisywane w ~/.config/CynamonCode/cynamoncode.conf."),
         tr("Po ponownym uruchomieniu program przywraca otwarte pliki (bez ich treści); "
            "funkcję można wyłączyć w menu Ustawienia."),
         tr("Ustawienia → Skróty klawiszowe pozwala zmienić skrót każdego polecenia menu."),
         tr("Ustawienia → Domyślny program dla plików: wybrane typy (np. HTML, CSS, "
            "JavaScript, PHP) otwierają się dwuklikiem w CynamonCode. Dotyczy tylko Twojego "
            "konta; odznaczenie przywraca poprzedni program.")});

    html += section(
        tr("Klawiatura"),
        {tr("F10 lub Alt otwiera menu; podkreślone litery wybierają polecenia."),
         tr("Ctrl+Tab i Ctrl+Shift+Tab przełączają karty, F8 przenosi kursor do drugiego "
            "widoku (domyślne skróty).")});

    if (!entries.isEmpty()) {
        html += QStringLiteral("<h3>%1</h3><table cellspacing=\"0\" cellpadding=\"3\">")
                    .arg(tr("Skróty klawiszowe").toHtmlEscaped());
        for (const ShortcutEntry &entry : entries) {
            if (entry.current.isEmpty())
                continue;
            html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td></tr>")
                        .arg(Shortcuts::displayText(entry.current).toHtmlEscaped(),
                             entry.title.toHtmlEscaped(), entry.category.toHtmlEscaped());
        }
        html += QStringLiteral("</table>");
    }

    html += QStringLiteral("<p>%1</p>")
                .arg(tr("Licencja: %1. Program jest dostarczany bez jakiejkolwiek gwarancji.")
                         .arg(AppInfo::licenseSpdx())
                         .toHtmlEscaped());
    m_browser->setHtml(html);
}

QString HelpDialog::html() const
{
    return m_browser->toHtml();
}

} // namespace cynamoncode
