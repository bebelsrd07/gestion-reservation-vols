#include "dialogconnexion.h"
#include "ui_dialogconnexion.h"
#include "databasemanager.h"
#include <QMessageBox>

DialogConnexion::DialogConnexion(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConnexion)
{
    ui->setupUi(this);
    setWindowTitle("Connexion");
}

DialogConnexion::~DialogConnexion()
{
    delete ui;
}

std::optional<Utilisateur> DialogConnexion::utilisateur() const
{
    return m_utilisateur;
}

void DialogConnexion::on_btnConnecter_clicked()
{
    QString email = ui->txtEmail->text().trimmed();
    QString mdp = ui->txtMotDePasse->text();

    if (email.isEmpty() || mdp.isEmpty()) {
        QMessageBox::warning(this, "Attention", "Veuillez remplir tous les champs.");
        return;
    }

    auto u = DatabaseManager::instance().authentifier(email, mdp);
    if (!u) {
        QMessageBox::warning(this, "Erreur", "Email ou mot de passe incorrect.");
        return;
    }

    m_utilisateur = u;
    accept();
}