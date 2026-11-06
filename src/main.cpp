// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Kamil Feliszewski

#include "app/AppInfo.h"
#include "ui/AboutDialog.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QTranslator>

#include <cstring>
#include <memory>

namespace {

// Interfejs programu jest po polsku; standardowe teksty Qt (okna wyboru pliku
// i czcionki, komunikaty wiersza poleceń) też, jeśli w systemie są tłumaczenia Qt
// (pakiet qt6-translations-l10n).
void installQtTranslator(QTranslator &translator)
{
    if (translator.load(QLocale(QLocale::Polish, QLocale::Poland), QStringLiteral("qtbase"),
                        QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(&translator);
    }
}

struct CommandLine
{
    QCommandLineParser parser;
    // Własne opcje zamiast addHelpOption()/addVersionOption(): tłumaczenie Qt opisów
    // tych opcji bywa niepełne, a interfejs ma być w całości po polsku.
    QCommandLineOption help{{QStringLiteral("h"), QStringLiteral("help")},
                            QCoreApplication::translate(
                                "main", "Wyświetla opis opcji i kończy działanie.")};
    QCommandLineOption version{{QStringLiteral("v"), QStringLiteral("version")},
                               QCoreApplication::translate(
                                   "main", "Wyświetla numer wersji i kończy działanie.")};

    CommandLine()
    {
        parser.setApplicationDescription(
            QCoreApplication::translate("main",
                                        "%1 — lekki edytor tekstu i kodu z kolorowaniem składni.")
                .arg(cynamoncode::AppInfo::productName()));
        parser.addOption(help);
        parser.addOption(version);
        parser.addPositionalArgument(QStringLiteral("pliki"),
                                     QCoreApplication::translate("main", "Pliki do otwarcia."),
                                     QStringLiteral("[pliki…]"));
    }

    // Nieznana opcja — komunikat i kod wyjścia 1; --help / --version kończą program.
    void process(const QCoreApplication &app)
    {
        parser.process(app);
        if (parser.isSet(help))
            parser.showHelp(0);
        if (parser.isSet(version))
            parser.showVersion();
    }
};

bool asksForTextOutput(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        for (const char *option : {"-h", "--help", "-v", "--version"}) {
            if (std::strcmp(argv[i], option) == 0)
                return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    // Nazwy wyznaczają ścieżkę ustawień: ~/.config/CynamonCode/cynamoncode.conf (D029).
    QCoreApplication::setOrganizationName(cynamoncode::AppInfo::productName());
    QCoreApplication::setApplicationName(QStringLiteral("cynamoncode"));
    QCoreApplication::setApplicationVersion(cynamoncode::AppInfo::version());

    // --help i --version działają bez serwera wyświetlania (np. przez SSH).
    if (asksForTextOutput(argc, argv)) {
        QCoreApplication app(argc, argv);
        QTranslator qtTranslator;
        installQtTranslator(qtTranslator);
        CommandLine commandLine;
        commandLine.process(app);
    }

    QApplication::setApplicationDisplayName(cynamoncode::AppInfo::productName());
    // Identyfikator wpisu w menu (D041) — środowisko dopasuje okno do pliku .desktop.
    QApplication::setDesktopFileName(cynamoncode::AppInfo::applicationId());

    QApplication app(argc, argv);
    QTranslator qtTranslator;
    installQtTranslator(qtTranslator);
    CommandLine commandLine;
    commandLine.process(app);

    QApplication::setWindowIcon(cynamoncode::applicationIcon());

    cynamoncode::MainWindow window({}, std::make_unique<QSettings>());
    window.show();

    // Najpierw poprzednia sesja (D034), potem pliki z wiersza poleceń.
    window.restoreSession();
    const QStringList files = commandLine.parser.positionalArguments();
    for (const QString &path : files)
        window.openFile(path);

    return QApplication::exec();
}
