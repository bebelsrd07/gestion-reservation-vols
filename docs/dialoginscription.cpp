#include "dialoginscription.h"
#include "ui_dialoginscription.h"
#include "databasemanager.h"
#include <QMessageBox>
#include <QRegularExpression>

DialogInscription::DialogInscription(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogInscription)
{
    ui->setupUi(this);
    setWindowTitle("Inscription");
}

DialogInscription::~DialogInscription()
{
    delete ui;
}

void DialogInscription::on_btnInscrire_clicked()
{
    QString nom = ui->txtNom->text().trimmed();
    QString prenom = ui->txtPrenom->text().trimmed();
    QString email = ui->txtEmail->text().trimmed();
    QString mdp = ui->txtMotDePasse->text();

    if (nom.isEmpty() || prenom.isEmpty() || email.isEmpty() || mdp.isEmpty()) {
        QMessageBox::warning(this, "Attention", "Veuillez remplir tous les champs.");
        return;
    }

    QRegularExpression regexEmail("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$");
    if (!regexEmail.match(email).hasMatch()) {
        QMessageBox::warning(this, "Attention", "Adresse email invalide.");
        return;
    }

    if (mdp.length() < 6) {
        QMessageBox::warning(this, "Attention",
                             "Le mot de passe doit contenir au moins 6 caractères.");
        return;
    }

    if (DatabaseManager::instance().emailExiste(email)) {
        QMessageBox::warning(this, "Attention", "Cet email est déjà utilisé.");
        return;
    }

    if (DatabaseManager::instance().inscrireUtilisateur(nom, prenom, email, mdp)) {
        QMessageBox::information(this, "Succès", "Inscription réussie !");
        accept();
    } else {
        QMessageBox::critical(this, "Erreur", "L'inscription a échoué.");
    }
}