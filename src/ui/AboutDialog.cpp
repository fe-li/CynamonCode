// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "ui/AboutDialog.h"

#include "app/AppInfo.h"

#include <Qsci/qsciglobal.h>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

// Zasoby z biblioteki statycznej trzeba zarejestrować jawnie (poza przestrzenią nazw).
static void initCynamonCodeResources()
{
    static bool initialized = false;
    if (!initialized) {
        Q_INIT_RESOURCE(cynamoncode);
        initialized = true;
    }
}

namespace cynamoncode {

QIcon applicationIcon()
{
    initCynamonCodeResources();
    QIcon icon;
    for (const int size : {16, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QStringLiteral(":/icons/cynamoncode-%1.png").arg(size), QSize(size, size));
    return icon;
}

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(tr("O programie %1").arg(AppInfo::productName()));
    setWindowIcon(applicationIcon());

    auto *logo = new QLabel(this);
    logo->setObjectName(QStringLiteral("logoLabel"));
    logo->setPixmap(applicationIcon().pixmap(QSize(128, 128)));
    logo->setAlignment(Qt::AlignTop);

    auto *name = new QLabel(AppInfo::productName(), this);
    name->setObjectName(QStringLiteral("nameLabel"));
    QFont nameFont = name->font();
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.8);
    nameFont.setBold(true);
    name->setFont(nameFont);

    auto *version = new QLabel(tr("Wersja %1").arg(AppInfo::version()), this);
    version->setObjectName(QStringLiteral("versionLabel"));

    auto *description =
        new QLabel(tr("Lekki edytor tekstu i kodu dla Linuksa — kolorowanie składni "
                      "bez podpowiadania i sprawdzania kodu."),
                   this);
    description->setObjectName(QStringLiteral("descriptionLabel"));
    description->setWordWrap(true);

    auto *author = new QLabel(tr("Autor: %1").arg(AppInfo::author()), this);
    author->setObjectName(QStringLiteral("authorLabel"));

    const QUrl url(AppInfo::website());
    auto *website = new QLabel(this);
    website->setObjectName(QStringLiteral("websiteLabel"));
    website->setTextFormat(Qt::RichText);
    website->setText(tr("Strona www: %1")
                         .arg(QStringLiteral("<a href=\"%1\">%2</a>")
                                  .arg(url.toString(), url.host().toHtmlEscaped())));
    website->setTextInteractionFlags(Qt::TextBrowserInteraction);
    website->setOpenExternalLinks(true);

    auto *license = new QLabel(
        tr("Licencja: GNU GPL w wersji 3 lub późniejszej (%1). Program jest dostarczany "
           "bez jakiejkolwiek gwarancji.")
            .arg(AppInfo::licenseSpdx()),
        this);
    license->setObjectName(QStringLiteral("licenseLabel"));
    license->setWordWrap(true);

    auto *libraries = new QLabel(
        tr("Qt %1, QScintilla %2").arg(QString::fromLatin1(qVersion()),
                                       QStringLiteral(QSCINTILLA_VERSION_STR)),
        this);
    libraries->setObjectName(QStringLiteral("librariesLabel"));
    QFont smallFont = libraries->font();
    smallFont.setPointSizeF(smallFont.pointSizeF() * 0.9);
    libraries->setFont(smallFont);

    auto *texts = new QVBoxLayout;
    texts->addWidget(name);
    texts->addWidget(version);
    texts->addSpacing(8);
    texts->addWidget(description);
    texts->addSpacing(8);
    texts->addWidget(author);
    texts->addWidget(website);
    texts->addSpacing(8);
    texts->addWidget(license);
    texts->addWidget(libraries);
    texts->addStretch();

    auto *content = new QHBoxLayout;
    content->addWidget(logo);
    content->addSpacing(16);
    content->addLayout(texts, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Zamknij"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(content);
    layout->addWidget(buttons);

    setMinimumWidth(460);
}

} // namespace cynamoncode
