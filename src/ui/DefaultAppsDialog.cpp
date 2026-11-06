// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/DefaultAppsDialog.h"

#include "app/AppInfo.h"
#include "services/MimeAppsService.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace cynamoncode {

QList<FileTypeGroup> DefaultAppsDialog::fileTypeGroups()
{
    // Nazwy języków nie podlegają tłumaczeniu, poza „Zwykły tekst”.
    return {
        {QStringLiteral("HTML"), {QStringLiteral("text/html")}},
        {QStringLiteral("CSS"), {QStringLiteral("text/css")}},
        {QStringLiteral("JavaScript"),
         {QStringLiteral("text/javascript"), QStringLiteral("application/javascript")}},
        {QStringLiteral("PHP"), {QStringLiteral("application/x-php"), QStringLiteral("text/x-php")}},
        {QStringLiteral("C/C++"),
         {QStringLiteral("text/x-c"), QStringLiteral("text/x-csrc"), QStringLiteral("text/x-chdr"),
          QStringLiteral("text/x-c++src"), QStringLiteral("text/x-c++hdr")}},
        {QStringLiteral("Python"), {QStringLiteral("text/x-python"), QStringLiteral("text/x-python3")}},
        {QStringLiteral("Java"), {QStringLiteral("text/x-java")}},
        {QStringLiteral("SQL"), {QStringLiteral("text/x-sql"), QStringLiteral("application/sql")}},
        {QStringLiteral("JSON"), {QStringLiteral("application/json")}},
        {QStringLiteral("XML"), {QStringLiteral("application/xml"), QStringLiteral("text/xml")}},
        {QStringLiteral("Bash"),
         {QStringLiteral("application/x-shellscript"), QStringLiteral("text/x-shellscript")}},
        {QStringLiteral("Markdown"), {QStringLiteral("text/markdown")}},
        {tr("Zwykły tekst"), {QStringLiteral("text/plain")}},
    };
}

DefaultAppsDialog::DefaultAppsDialog(const MimeAppsService &service, bool installed,
                                     QWidget *parent)
    : QDialog(parent)
    , m_groups(fileTypeGroups())
{
    setObjectName(QStringLiteral("defaultAppsDialog"));
    setWindowTitle(tr("Domyślny program dla plików"));

    auto *intro = new QLabel(
        tr("Zaznaczone typy plików będą otwierane dwuklikiem w %1. Zmiana dotyczy tylko "
           "Twojego konta; odznaczenie przywraca poprzedni program (np. przeglądarkę dla "
           "HTML).")
            .arg(AppInfo::productName()),
        this);
    intro->setWordWrap(true);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("typeList"));
    m_list->setAccessibleName(tr("Typy plików otwierane w %1").arg(AppInfo::productName()));
    for (const FileTypeGroup &group : std::as_const(m_groups)) {
        int defaults = 0;
        for (const QString &mimeType : group.mimeTypes)
            defaults += service.isDefault(mimeType) ? 1 : 0;
        const Qt::CheckState state = defaults == 0 ? Qt::Unchecked
                                     : defaults == group.mimeTypes.size() ? Qt::Checked
                                                                          : Qt::PartiallyChecked;
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  (%2)").arg(group.name, group.mimeTypes.join(QStringLiteral(", "))),
            m_list);
        item->setData(Qt::UserRole, group.name);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(state);
        m_initial.append(state);
    }
    m_list->setCurrentRow(0);

    auto *warning = new QLabel(this);
    warning->setObjectName(QStringLiteral("notInstalledLabel"));
    warning->setWordWrap(true);
    warning->setText(tr("Program nie jest zainstalowany z pakietu (brak wpisu %1 w menu), "
                        "więc system nie może go uruchamiać dla plików.")
                         .arg(service.desktopId()));
    warning->setVisible(!installed);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Anuluj"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    // Bez instalacji można tylko zdjąć wcześniejsze ustawienie, nie dodać nowego.
    const auto updateOk = [this, buttons, installed] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(installed || typesToEnable().isEmpty());
    };
    connect(m_list, &QListWidget::itemChanged, this, updateOk);
    updateOk();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_list, 1);
    layout->addWidget(warning);
    layout->addWidget(buttons);
    resize(520, 460);
}

QStringList DefaultAppsDialog::groupNames() const
{
    QStringList names;
    for (const FileTypeGroup &group : m_groups)
        names.append(group.name);
    return names;
}

QListWidgetItem *DefaultAppsDialog::itemFor(const QString &group) const
{
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(Qt::UserRole).toString() == group)
            return m_list->item(row);
    }
    return nullptr;
}

Qt::CheckState DefaultAppsDialog::checkState(const QString &group) const
{
    const QListWidgetItem *item = itemFor(group);
    return item ? item->checkState() : Qt::Unchecked;
}

bool DefaultAppsDialog::setChecked(const QString &group, bool checked)
{
    QListWidgetItem *item = itemFor(group);
    if (!item)
        return false;
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return true;
}

QStringList DefaultAppsDialog::typesToEnable() const
{
    QStringList types;
    for (int row = 0; row < m_groups.size(); ++row) {
        const Qt::CheckState state = m_list->item(row)->checkState();
        if (state == Qt::Checked && m_initial.at(row) != Qt::Checked)
            types += m_groups.at(row).mimeTypes;
    }
    return types;
}

QStringList DefaultAppsDialog::typesToDisable() const
{
    QStringList types;
    for (int row = 0; row < m_groups.size(); ++row) {
        const Qt::CheckState state = m_list->item(row)->checkState();
        if (state == Qt::Unchecked && m_initial.at(row) != Qt::Unchecked)
            types += m_groups.at(row).mimeTypes;
    }
    return types;
}

} // namespace cynamoncode
