#include "dialogreservation.h"
#include "ui_dialogreservation.h"

DialogReservation::DialogReservation(const QString &numeroVol, double prixUnitaire,
                                     int placesDisponibles, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogReservation)
    , m_prixUnitaire(prixUnitaire)
{
    ui->setupUi(this);
    setWindowTitle("Réservation");

    ui->lblVol->setText("Vol " + numeroVol + " - "
                        + QString::number(prixUnitaire, 'f', 2) + " € par billet");
    ui->spinBillets->setMinimum(1);
    ui->spinBillets->setMaximum(placesDisponibles);
    ui->spinBillets->setValue(1);
    on_spinBillets_valueChanged(1);
}

DialogReservation::~DialogReservation()
{
    delete ui;
}

int DialogReservation::nombreBillets() const
{
    return ui->spinBillets->value();
}

void DialogReservation::on_spinBillets_valueChanged(int valeur)
{
    double total = m_prixUnitaire * valeur;
    ui->lblTotal->setText("Total : " + QString::number(total, 'f', 2) + " €");
}

void DialogReservation::on_btnConfirmer_clicked()
{
    accept();
}