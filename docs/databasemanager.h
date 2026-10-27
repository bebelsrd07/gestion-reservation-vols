#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QString>
#include <optional>
#include "utilisateur.h"

class DatabaseManager {
private:
    QSqlDatabase m_db;
    DatabaseManager();

public:
    static DatabaseManager& instance();
    bool connecter();
    void deconnecter();
    bool estConnecte() const;

    // Gestion des utilisateurs
    bool emailExiste(const QString &email);
    bool inscrireUtilisateur(const QString &nom, const QString &prenom,
                             const QString &email, const QString &motDePasse);
    std::optional<Utilisateur> authentifier(const QString &email,
                                            const QString &motDePasse);

    // Gestion des réservations
    bool creerReservation(int idUtilisateur, int idVol, int nombreBillets,
                          QString &reference);
    bool annulerReservation(int idReservation, int idUtilisateur);
};