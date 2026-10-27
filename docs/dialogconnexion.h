#pragma once

#include <QDialog>
#include <optional>
#include "utilisateur.h"

namespace Ui {
class DialogConnexion;
}

class DialogConnexion : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConnexion(QWidget *parent = nullptr);
    ~DialogConnexion();

    std::optional<Utilisateur> utilisateur() const;

private slots:
    void on_btnConnecter_clicked();

private:
    Ui::DialogConnexion *ui;
    std::optional<Utilisateur> m_utilisateur;
};