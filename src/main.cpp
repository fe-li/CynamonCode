#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // applicationName wyznacza ścieżki ustawień (QSettings) — małymi literami jak plik wykonywalny.
    QApplication::setApplicationName(QStringLiteral("cynamoncode"));
    QApplication::setApplicationDisplayName(QStringLiteral("CynamonCode"));
    QApplication::setApplicationVersion(QStringLiteral(CYNAMONCODE_VERSION));

    QApplication app(argc, argv);

    cynamoncode::MainWindow window;
    window.show();

    return QApplication::exec();
}
