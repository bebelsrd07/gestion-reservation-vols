#pragma once

#include <QString>

class Utilisateur
{
public:
    Utilisateur(int id, const QString &nom, const QString &prenom,
                const QString &email, const QString &motDePasseHache,
                const QString &role = "Client");

    int getIdUtilisateur() const;
    QString getNom() const;
    QString getPrenom() const;
    QString getEmail() const;
    QString getRole() const;

    bool estAdmin() const;

    // Retourne le mot de passe haché en SHA-256 (hexadécimal)
    static QString hacherMotDePasse(const QString &motDePasse);

    // Vérifie si un mot de passe saisi correspond au mot de passe stocké
    bool verifierMotDePasse(const QString &motDePasseSaisi) const;

private:
    int m_idUtilisateur;
    QString m_nom;
    QString m_prenom;
    QString m_email;
    QString m_motDePasseHache;
    QString m_role;
};