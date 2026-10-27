#pragma once

#include <QDialog>

namespace Ui {
class DialogInscription;
}

class DialogInscription : public QDialog
{
    Q_OBJECT

public:
    explicit DialogInscription(QWidget *parent = nullptr);
    ~DialogInscription();

private slots:
    void on_btnInscrire_clicked();

private:
    Ui::DialogInscription *ui;
};