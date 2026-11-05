#include "ui/MainWindow.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

namespace cynamoncode {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("MainWindow"));
    // Nazwa produktu nie podlega tłumaczeniu.
    setWindowTitle(QStringLiteral("CynamonCode"));
    resize(1024, 720);

    createMenus();
    statusBar()->showMessage(tr("Gotowy"));
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&Plik"));
    fileMenu->setObjectName(QStringLiteral("menuFile"));

    QAction *quitAction = fileMenu->addAction(tr("Za&kończ"));
    quitAction->setObjectName(QStringLiteral("actionQuit"));
    // Jawny skrót: QKeySequence::Quit bywa pusty zależnie od motywu platformy.
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quitAction->setStatusTip(tr("Zamyka program"));
    // close(), a nie QApplication::quit(): w E1 closeEvent zapyta o niezapisane zmiany.
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

} // namespace cynamoncode
