#include "app/AppInfo.h"
#include "ui/AboutDialog.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QSettings>

#include <memory>

int main(int argc, char *argv[])
{
    // Nazwy wyznaczają ścieżkę ustawień: ~/.config/CynamonCode/cynamoncode.conf (D029).
    QApplication::setOrganizationName(cynamoncode::AppInfo::productName());
    QApplication::setApplicationName(QStringLiteral("cynamoncode"));
    QApplication::setApplicationDisplayName(cynamoncode::AppInfo::productName());
    QApplication::setApplicationVersion(cynamoncode::AppInfo::version());
    // Nazwa pliku .desktop (E5) — środowisko dopasuje okno do wpisu w menu.
    QApplication::setDesktopFileName(QStringLiteral("cynamoncode"));

    QApplication app(argc, argv);
    QApplication::setWindowIcon(cynamoncode::applicationIcon());

    cynamoncode::MainWindow window({}, std::make_unique<QSettings>());
    window.show();

    return QApplication::exec();
}
