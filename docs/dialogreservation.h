#pragma once

#include <QDialog>
#include <QString>

namespace Ui {
class DialogReservation;
}

class DialogReservation : public QDialog
{
    Q_OBJECT

public:
    DialogReservation(const QString &numeroVol, double prixUnitaire,
                      int placesDisponibles, QWidget *parent = nullptr);
    ~DialogReservation();

    int nombreBillets() const;

private slots:
    void on_spinBillets_valueChanged(int valeur);
    void on_btnConfirmer_clicked();

private:
    Ui::DialogReservation *ui;
    double m_prixUnitaire;
};