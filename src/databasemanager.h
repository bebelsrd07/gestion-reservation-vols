#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include "modeles.h"
#include <QHash>
#include <QPair>
#include <QSet>
#include <QSqlDatabase>
#include <QString>

class QSqlQueryModel;

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool connecter();
    void deconnecter();
    bool estConnecte() const;
    QSqlDatabase database() const { return m_db; }
    QString derniereErreur() const { return m_derniereErreur; }

    // --- Authentification et gestion des comptes ---
    QString genererSelAleatoire() const;
    QString hacherMotDePasse(const QString &motDePasse, const QString &sel) const;

    bool connecterUtilisateur(const QString &email,
                              const QString &motDePasse,
                              int &idUtilisateur,
                              QString &role,
                              QString &nomComplet,
                              QString &emailNormalise,
                              QString &erreur);

    bool inscrireClient(const QString &nom,
                        const QString &prenom,
                        const QString &email,
                        const QString &motDePasse,
                        QString &erreur);

    // CRUD Administrateurs réservé exclusivement au Super Administrateur
    bool creerAdministrateur(const QString &roleAppelant,
                             const QString &nom,
                             const QString &prenom,
                             const QString &email,
                             const QString &motDePasse,
                             QString &erreur);

    bool modifierAdministrateur(const QString &roleAppelant,
                                int idAdminCible,
                                const QString &nom,
                                const QString &prenom,
                                const QString &email,
                                const QString &nouveauMotDePasseOptionnel,
                                QString &erreur);

    bool supprimerAdministrateur(const QString &roleAppelant,
                                 int idAdminCible,
                                 QString &erreur);

    // Gestion et assistance des comptes Clients par Admin ou Super Admin
    bool modifierClientParAdmin(const QString &roleAppelant,
                                int idClientCible,
                                const QString &nom,
                                const QString &prenom,
                                const QString &email,
                                bool estBloque,
                                const QString &nouveauMotDePasseOptionnel,
                                QString &erreur);

    bool supprimerClientParAdmin(const QString &roleAppelant,
                                 int idClientCible,
                                 QString &erreur);

    // --- Données de référence (villes, aéroports, liaisons) ---
    QList<AeroportInfo> chargerAeroports();
    QHash<QPair<int, int>, LiaisonReference> chargerLiaisons();

    // --- Gestion des vols et plan de cabine ---
    QList<TronconVol> chargerVolsProgrammes();
    QSet<QString> chargerSiegesOccupesVol(int idVol);
    QString genererNumeroVolUnique(const QString &codePaysDepart = QStringLiteral("MG"));

    bool ajouterVolManuel(const QString &compagnie,
                          int idAeroportDepart,
                          int idAeroportArrivee,
                          const QDateTime &dateDepart,
                          int capacite,
                          double prixForce,
                          QString &numeroCree,
                          QString &erreur);

    bool modifierVolManuel(int idVol,
                           const QString &compagnie,
                           int idAeroportDepart,
                           int idAeroportArrivee,
                           const QDateTime &dateDepart,
                           int capacite,
                           double prixForce,
                           bool notifierClients,
                           QStringList &referencesReservationsImpactees,
                           QString &erreur);

    bool supprimerVol(int idVol, QString &erreur);

    // --- Réservations multi-passagers et validation par l'admin ---
    bool creerReservation(int idUtilisateur,
                          const ItinerairePropose &itineraire,
                          const QList<PassagerInfo> &passagers,
                          QString &referenceCreee,
                          QStringList &numerosVolsFinaux,
                          QString &erreur,
                          double supplementSiegesTotal = 0.0,
                          const InfosPaiement &paiement = InfosPaiement());

    bool changerStatutReservation(const QString &reference,
                                  const QString &nouveauStatut,
                                  QString &erreur);

    bool annulerReservationClient(const QString &reference,
                                  int idUtilisateur,
                                  QString &erreur);

    bool obtenirDetailsReservation(const QString &reference,
                                   DetailsReservation &details,
                                   QString &erreur);

    // --- Boîte de réception & notifications multi-rôles ---
    bool envoyerMessageBoite(int idDestinataire,
                             const QString &roleCible,
                             int idExpediteur,
                             const QString &nomExpediteur,
                             const QString &emailExpediteur,
                             const QString &categorie,
                             const QString &sujet,
                             const QString &contenu,
                             QString &erreur);

    QList<MessageBoiteReception> chargerMessagesBoite(int idUtilisateur,
                                                      const QString &role,
                                                      bool seulementNonLus = false);
    int compterMessagesNonLus(int idUtilisateur, const QString &role);
    void marquerMessageCommeLu(int idMessage);
    void toutMarquerCommeLu(int idUtilisateur, const QString &role);
    void supprimerMessageBoite(int idMessage);
    void viderBoiteReception(int idUtilisateur, const QString &role);

    // --- Tableau de bord, export CSV et notifications multi-instances ---
    StatistiquesAdmin obtenirStatistiquesAdmin();
    QString empreinteReservationsClient(int idUtilisateur);
    bool exporterReservationsCsv(const QString &cheminCsv, QString &erreur);

    // --- Modèles tabulaires prêts pour les QTableView ---
    void remplirModeleReservationsClient(QSqlQueryModel *modele,
                                         int idUtilisateur,
                                         const QString &codeMonnaie = QStringLiteral("EUR"));
    void remplirModeleReservationsAdmin(QSqlQueryModel *modele,
                                        const QString &filtreStatut,
                                        const QString &codeMonnaie = QStringLiteral("EUR"));
    void remplirModeleVolsAdmin(QSqlQueryModel *modele,
                                const QString &codeMonnaie = QStringLiteral("EUR"));
    void remplirModeleAdministrateurs(QSqlQueryModel *modele);
    void remplirModeleClientsAdmin(QSqlQueryModel *modele, const QString &filtreRecherche = QString());

    static QString cheminFichierConfig();

private:
    void assurerSchemaComplementaire();

    QSqlDatabase m_db;
    QString m_derniereErreur;
};

#endif // DATABASEMANAGER_H
