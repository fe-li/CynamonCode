#pragma once

#include <QDialog>
#include <QIcon>

namespace cynamoncode {

// Okno „O programie”: logo, nazwa, wersja, autor i strona www. Link otwiera
// przeglądarkę dopiero po kliknięciu — program sam nie łączy się z siecią.
class AboutDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

// Ikona programu ze wszystkimi rozmiarami z zasobów (okno, pasek zadań).
QIcon applicationIcon();

} // namespace cynamoncode
