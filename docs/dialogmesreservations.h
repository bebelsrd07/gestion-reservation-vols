#pragma once

#include <QDialog>

namespace Ui {
class DialogMesReservations;
}

class DialogMesReservations : public QDialog
{
    Q_OBJECT

public:
    DialogMesReservations(int idUtilisateur, QWidget *parent = nullptr);
    ~DialogMesReservations();

private slots:
    void on_btnAnnuler_clicked();

private:
    Ui::DialogMesReservations *ui;
    int m_idUtilisateur;
    void charger();
};