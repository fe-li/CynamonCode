#pragma once

#include <QMainWindow>

namespace cynamoncode {

// Główne okno aplikacji. W E0 zawiera tylko menu „Plik → Zakończ” i pasek stanu;
// edytor, karty i operacje na plikach dochodzą w kolejnych etapach.
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createMenus();
};

} // namespace cynamoncode
