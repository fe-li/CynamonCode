#include "ui/MainWindow.h"

#include <QApplication>
#include <QSettings>

#include <memory>

int main(int argc, char *argv[])
{
    // Nazwy wyznaczają ścieżkę ustawień: ~/.config/CynamonCode/cynamoncode.conf (D029).
    QApplication::setOrganizationName(QStringLiteral("CynamonCode"));
    QApplication::setApplicationName(QStringLiteral("cynamoncode"));
    QApplication::setApplicationDisplayName(QStringLiteral("CynamonCode"));
    QApplication::setApplicationVersion(QStringLiteral(CYNAMONCODE_VERSION));

    QApplication app(argc, argv);

    cynamoncode::MainWindow window({}, std::make_unique<QSettings>());
    window.show();

    return QApplication::exec();
}
