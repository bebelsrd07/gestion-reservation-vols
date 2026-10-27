#include "utilisateur.h"
#include <QCryptographicHash>

Utilisateur::Utilisateur(int id, const QString &nom, const QString &prenom,
                         const QString &email, const QString &motDePasseHache,
                         const QString &role)
    : m_idUtilisateur(id)
    , m_nom(nom)
    , m_prenom(prenom)
    , m_email(email)
    , m_motDePasseHache(motDePasseHache)
    , m_role(role)
{
}

int Utilisateur::getIdUtilisateur() const { return m_idUtilisateur; }
QString Utilisateur::getNom() const { return m_nom; }
QString Utilisateur::getPrenom() const { return m_prenom; }
QString Utilisateur::getEmail() const { return m_email; }
QString Utilisateur::getRole() const { return m_role; }

bool Utilisateur::estAdmin() const
{
    return m_role == "Admin";
}

QString Utilisateur::hacherMotDePasse(const QString &motDePasse)
{
    QByteArray hash = QCryptographicHash::hash(motDePasse.toUtf8(),
                                               QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

bool Utilisateur::verifierMotDePasse(const QString &motDePasseSaisi) const
{
    return hacherMotDePasse(motDePasseSaisi) == m_motDePasseHache;
}