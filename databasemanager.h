#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QList>
#include "vol.h"
#include "reservation.h"
#include <QDateTime>
#include <QString>

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool connecter();
    void deconnecter();
    bool estConnecte() const;
    QSqlDatabase database() const { return m_db; }

    QList<Vol> obtenirTousLesVols();
    bool enregistrerReservation(const Reservation &r, int idUtilisateur,
                                const QList<int> &sieges = QList<int>());
    bool enregistrerItineraire(int idVol1, int idVol2, const QString &passager,
                               int nb, const QString &typeBillet,
                               int idUtilisateur, QString &refGroupe,
                               const QList<int> &sieges1 = QList<int>(),
                               const QList<int> &sieges2 = QList<int>());
    QList<int> siegesAutomatiques(int idVol, int nb);
    bool simulerPerturbation(int idVol, const QString &type, int minutes,
                             int &nbAlertes);
    int nombreAlertesNonLues(int idUtilisateur);
    bool annulerReservation(const QString &reference);
    bool ajouterVol(const QString &numero, const QString &compagnie,
                    const QString &depart, const QString &arrivee,
                    const QDateTime &dateDepart, const QDateTime &dateArrivee,
                    double prix, int places);
    bool modifierVol(int idVol, const QString &numero, const QString &compagnie,
                     const QString &depart, const QString &arrivee,
                     const QDateTime &dateDepart, const QDateTime &dateArrivee,
                     double prix, int places);
    bool supprimerVol(int idVol);
    bool changerStatutReservation(const QString &reference, const QString &nouveauStatut);
    int nombreVols();
    int nombreReservations(const QString &statut = QString());

    // --- Comptes utilisateurs ---
    QString genererSel();
    QString hacherMotDePasse(const QString &motDePasse, const QString &sel);
    bool inscrireUtilisateur(const QString &nom, const QString &prenom,
                             const QString &email, const QString &motDePasse,
                             const QString &role = "client");
    bool connecterUtilisateur(const QString &email, const QString &motDePasse,
                              int &idUtilisateur, QString &role,
                              QString &nomComplet);

private:
      QSqlDatabase m_db;
    bool attribuerSieges(int idReservation, int idVol, int nb,
                         const QList<int> &sieges, bool facturer = true);
    bool genererSieges(int idVol, int de, int a);

};

#endif // DATABASEMANAGER_H