#include "logindialog.h"
#include "databasemanager.h"
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QMessageBox>

LoginDialog::LoginDialog(DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle("Réservation de vols - Accès");
    resize(380, 300);

    m_onglets = new QTabWidget(this);

    // ----- Onglet Connexion -----
    QWidget *pageLogin = new QWidget;
    QFormLayout *formLogin = new QFormLayout;
    m_loginEmail = new QLineEdit;
    m_loginMdp = new QLineEdit;
    m_loginMdp->setEchoMode(QLineEdit::Password);
    formLogin->addRow("Email :", m_loginEmail);
    formLogin->addRow("Mot de passe :", m_loginMdp);
    QPushButton *btnLogin = new QPushButton("Se connecter");
    btnLogin->setDefault(true);
    QVBoxLayout *vLogin = new QVBoxLayout(pageLogin);
    vLogin->addLayout(formLogin);
    vLogin->addWidget(btnLogin);
    vLogin->addStretch();

    // ----- Onglet Inscription -----
    QWidget *pageInscr = new QWidget;
    QFormLayout *formInscr = new QFormLayout;
    m_nomEdit = new QLineEdit;
    m_prenomEdit = new QLineEdit;
    m_emailEdit = new QLineEdit;
    m_mdpEdit = new QLineEdit;
    m_mdp2Edit = new QLineEdit;
    m_mdpEdit->setEchoMode(QLineEdit::Password);
    m_mdp2Edit->setEchoMode(QLineEdit::Password);
    formInscr->addRow("Nom :", m_nomEdit);
    formInscr->addRow("Prénom :", m_prenomEdit);
    formInscr->addRow("Email :", m_emailEdit);
    formInscr->addRow("Mot de passe :", m_mdpEdit);
    formInscr->addRow("Confirmation :", m_mdp2Edit);
    QPushButton *btnInscr = new QPushButton("Créer mon compte");
    QVBoxLayout *vInscr = new QVBoxLayout(pageInscr);
    vInscr->addLayout(formInscr);
    vInscr->addWidget(btnInscr);
    vInscr->addStretch();

    m_onglets->addTab(pageLogin, "Connexion");
    m_onglets->addTab(pageInscr, "Inscription");

    QPushButton *btnVisiteur = new QPushButton("Continuer en tant que visiteur (consultation seule)");
    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->addWidget(m_onglets);
    principal->addWidget(btnVisiteur);

    connect(btnVisiteur, &QPushButton::clicked, this, [this]() {
        m_id = 0;
        m_role = "visiteur";
        m_nom = "Visiteur";
        accept();
    });

    connect(btnLogin, &QPushButton::clicked, this, &LoginDialog::seConnecter);
    connect(m_loginMdp, &QLineEdit::returnPressed, this, &LoginDialog::seConnecter);
    connect(btnInscr, &QPushButton::clicked, this, &LoginDialog::sInscrire);
}

void LoginDialog::seConnecter()
{
    int id = 0;
    QString role, nom;
    if (m_db->connecterUtilisateur(m_loginEmail->text(), m_loginMdp->text(),
                                   id, role, nom)) {
        m_id = id;
        m_role = role;
        m_nom = nom;
        accept();                           // ferme la fenêtre avec succès
    } else {
        QMessageBox::warning(this, "Connexion",
                             "Email ou mot de passe incorrect.");
        m_loginMdp->clear();
    }
}

void LoginDialog::sInscrire()
{
    if (m_mdpEdit->text() != m_mdp2Edit->text()) {
        QMessageBox::warning(this, "Inscription",
                             "Les deux mots de passe ne sont pas identiques.");
        return;
    }
    if (m_db->inscrireUtilisateur(m_nomEdit->text(), m_prenomEdit->text(),
                                  m_emailEdit->text(), m_mdpEdit->text())) {
        QMessageBox::information(this, "Inscription",
                                 "Compte créé ! Vous pouvez maintenant vous connecter.");
        m_loginEmail->setText(m_emailEdit->text().trimmed());
        m_loginMdp->clear();
        m_nomEdit->clear();
        m_prenomEdit->clear();
        m_emailEdit->clear();
        m_mdpEdit->clear();
        m_mdp2Edit->clear();
        m_onglets->setCurrentIndex(0);      // retour à l'onglet Connexion
    } else {
        QMessageBox::warning(this, "Inscription",
                             "Inscription impossible. Vérifiez que :\n"
                             "- tous les champs sont remplis ;\n"
                             "- l'email est valide et pas déjà utilisé ;\n"
                             "- le mot de passe fait au moins 6 caractères.");
    }
}