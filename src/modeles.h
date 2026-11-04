#ifndef MODELES_H
#define MODELES_H

#include <QString>
#include <QDate>
#include <QDateTime>
#include <QList>

// Structures simples pour transporter les données entre MySQL, les calculs et l'interface.

struct AeroportInfo {
    int idAeroport = 0;
    QString codeIata;
    QString nomAeroport;
    QString ville;
    QString pays;
    QString codePays;
    double latitude = 0.0;
    double longitude = 0.0;
    bool estHub = false;

    bool estValide() const { return idAeroport > 0; }

    // Format demandé dans les barres de recherche : "Ville, Pays  CC"
    QString affichageVillePays() const
    {
        return QStringLiteral("%1, %2  %3").arg(ville, pays, codePays);
    }

    QString affichageAeroportCourt() const
    {
        return QStringLiteral("%1 - %2").arg(codeIata, nomAeroport);
    }
};

struct LiaisonReference {
    bool trouvee = false;
    int distanceKm = 0;
    int dureeMinutes = 0;
    double prixBase = 0.0;
};

struct TronconVol {
    int idVolExistant = 0; // 0 si le vol sera créé automatiquement à la réservation
    QString numeroVol;
    QString compagnie = QStringLiteral("Madagascar Airlines");
    AeroportInfo depart;
    AeroportInfo arrivee;
    QDateTime dateDepart;
    QDateTime dateArrivee;
    int distanceKm = 0;
    int dureeMinutes = 0;
    double prixUnitaire = 0.0;
    int placesDisponibles = 0;
};

struct ItinerairePropose {
    QList<TronconVol> troncons;
    int distanceTotaleKm = 0;
    int dureeTotaleMinutes = 0;
    double prixParPassager = 0.0;
    int placesMinDisponibles = 0;

    int nbEscales() const
    {
        return troncons.isEmpty() ? 0 : (troncons.size() - 1);
    }

    QDateTime dateDepartInitiale() const
    {
        return troncons.isEmpty() ? QDateTime() : troncons.first().dateDepart;
    }

    QDateTime dateArriveeFinale() const
    {
        return troncons.isEmpty() ? QDateTime() : troncons.last().dateArrivee;
    }

    QString trajetCourt() const
    {
        if (troncons.isEmpty())
            return QString();
        return QStringLiteral("%1 (%2) → %3 (%4)")
            .arg(troncons.first().depart.ville,
                 troncons.first().depart.codeIata,
                 troncons.last().arrivee.ville,
                 troncons.last().arrivee.codeIata);
    }

    QString resumeTrajet() const
    {
        if (troncons.isEmpty())
            return QString();
        QStringList etapes;
        etapes << QStringLiteral("%1 (%2)").arg(troncons.first().depart.ville,
                                                troncons.first().depart.codeIata);
        for (const TronconVol &t : troncons) {
            etapes << QStringLiteral("%1 (%2)").arg(t.arrivee.ville, t.arrivee.codeIata);
        }
        return etapes.join(QStringLiteral(" → "));
    }

    QString descriptionEscales() const
    {
        if (troncons.size() <= 1)
            return QStringLiteral("Direct");
        QStringList villesEscales;
        for (int i = 0; i < troncons.size() - 1; ++i) {
            int attenteMin = static_cast<int>(
                troncons[i].dateArrivee.secsTo(troncons[i + 1].dateDepart) / 60);
            villesEscales << QStringLiteral("%1 (%2h%3)")
                                 .arg(troncons[i].arrivee.ville)
                                 .arg(attenteMin / 60)
                                 .arg(attenteMin % 60, 2, 10, QChar('0'));
        }
        return QStringLiteral("%1 esc. : %2")
            .arg(villesEscales.size())
            .arg(villesEscales.join(QStringLiteral(", ")));
    }

    QString numerosVolsAffiches() const
    {
        QStringList nums;
        for (const TronconVol &t : troncons) {
            nums << (t.numeroVol.isEmpty() ? QStringLiteral("Auto") : t.numeroVol);
        }
        return nums.join(QStringLiteral(" + "));
    }
};

struct PassagerInfo {
    QString nom;
    QString prenom;
    QDate dateNaissance;
    QString numeroPasseport;
    QString siege;
};

struct InfosPaiement {
    QString modePaiement;      // "Mobile Money" ou "Carte bancaire"
    QString contactPaiement;   // N° envoyeur ou Titulaire + 4 derniers chiffres
    QString referencePaiement; // Référence SMS ou code reçu/autorisation
};

struct DetailsReservation {
    int idReservation = 0;
    QString reference;
    int idUtilisateur = 0;
    QString nomClient;
    QString prenomClient;
    QString emailClient;
    int nbPlaces = 0;
    double prixTotal = 0.0;
    QString statut;
    int nbEscales = 0;
    QString modePaiement;
    QString contactPaiement;
    QString referencePaiement;
    QDateTime dateReservation;
    QList<TronconVol> troncons;
    QList<PassagerInfo> passagers;
};

struct StatistiquesAdmin {
    int nbVolsProgrammes = 0;
    int totalReservations = 0;
    int nbEnAttente = 0;
    int nbConfirmees = 0;
    int nbAnnulees = 0;
    double chiffreAffairesConfirme = 0.0;
    int dernierIdEnAttente = 0;
};

struct MessageBoiteReception {
    int idMessage = 0;
    int idDestinataire = 0;
    QString roleCible;
    int idExpediteur = 0;
    QString nomExpediteur;
    QString emailExpediteur;
    QString categorie;
    QString sujet;
    QString contenu;
    bool estLu = false;
    QDateTime dateEnvoi;
};

#endif // MODELES_H
