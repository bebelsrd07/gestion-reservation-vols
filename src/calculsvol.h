#ifndef CALCULSVOL_H
#define CALCULSVOL_H

#include "modeles.h"
#include <QHash>
#include <QPair>

// Fonctions pures de calcul aéronautique, d'itinéraires avec escales et de validation.
namespace CalculsVol {

    // Calcul de la distance orthodromique en kilomètres entre deux coordonnées GPS
    int calculerDistanceHaversineKm(double lat1, double lon1, double lat2, double lon2);

    // Durée estimée d'un vol en minutes à partir de la distance
    int calculerDureeVolMinutes(int distanceKm);

    // Tarif de référence par passager en EUR à partir de la distance
    double calculerPrixVolUnitaire(int distanceKm);

    // Formatage lisible d'une durée (ex : 145 min -> "2 h 25")
    QString formaterDureeMinutes(int minutes);

    // Calcul des caractéristiques d'un tronçon entre deux aéroports (table liaison sinon Haversine)
    LiaisonReference calculerReferenceTroncon(
        const AeroportInfo &depart,
        const AeroportInfo &arrivee,
        const QHash<QPair<int, int>, LiaisonReference> &liaisonsConnues);

    // Recherche combinée : vols programmés en base + itinéraires automatiques (0, 1 ou 2 escales max)
    QList<ItinerairePropose> rechercherItineraires(
        const QList<AeroportInfo> &aeroportsDepart,
        const QList<AeroportInfo> &aeroportsArrivee,
        const QList<AeroportInfo> &tousLesAeroports,
        const QHash<QPair<int, int>, LiaisonReference> &liaisonsConnues,
        const QList<TronconVol> &volsProgrammes,
        bool filtrerParDate,
        const QDate &dateDepartSouhaitee,
        int nbPassagers,
        bool filtrerPrixMax,
        double prixMaxParPassager,
        const QString &villeEscaleObligatoire);

    // Validation stricte d'adresse email (refus des alias '+' et des domaines temporaires)
    bool validerEmailStrict(const QString &email, QString &messageErreur);

    // Normalisation simple (sans accents, majuscules, espaces nettoyés) pour comparer deux textes
    QString normaliserTexteIdentite(const QString &texte);

    // Lecture sans OCR des métadonnées intégrées dans un fichier passeport PDF ou PNG
    bool lireFichierPasseportSansOcr(const QString &cheminFichier,
                                     QString &numeroPasseport,
                                     QString &nom,
                                     QString &prenom,
                                     QString &messageErreur);

} // namespace CalculsVol

#endif // CALCULSVOL_H
