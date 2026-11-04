#ifndef CONSTANTES_H
#define CONSTANTES_H

#include <QStringList>

// Je regroupe toutes les constantes métier et techniques ici pour éviter
// tout nombre magique dans les calculs et faciliter les ajustements.
namespace Constantes {

    // --- Géométrie terrestre et formule de Haversine ---
    constexpr double PI_VALEUR = 3.14159265358979323846;
    constexpr double DEGRES_VERS_RADIANS = PI_VALEUR / 180.0;
    constexpr double RAYON_TERRE_KM = 6371.0;

    // --- Calcul automatique de la durée d'un vol ---
    constexpr double VITESSE_CROISIERE_KMH = 800.0;
    constexpr double MINUTES_PAR_HEURE = 60.0;
    constexpr int FORFAIT_ROULAGE_APPROCHE_MIN = 30;
    constexpr int DUREE_VOL_MINIMALE_MIN = 45;

    // --- Calcul automatique du tarif par passager (en EUR) ---
    constexpr double PRIX_FIXE_BASE_EUR = 35.0;
    constexpr double TARIF_PAR_KM_EUR = 0.085;
    constexpr double FACTEUR_ARRONDI_CENTIMES = 100.0;

    // --- Règles de cohérence des escales (1 à 2 escales maximum) ---
    constexpr int NB_ESCALES_MAX = 2;
    constexpr int ESCALE_DUREE_MIN_MINUTES = 60;
    constexpr int ESCALE_DUREE_MAX_MINUTES = 720;
    constexpr int ESCALE_DUREE_STANDARD_MINUTES = 120;

    // --- Seuils de distance pour la recherche automatique d'itinéraires ---
    constexpr double DISTANCE_MAX_VOL_DIRECT_KM = 9800.0;
    constexpr double RATIO_DETOUR_MAX_UNE_ESCALE = 1.65;
    constexpr double RATIO_DETOUR_MAX_DEUX_ESCALES = 1.95;
    constexpr int MAX_ITINERAIRES_CALCULES_PAR_TYPE = 3;

    // --- Horaires par défaut des vols calculés à la demande ---
    constexpr int HEURE_DEPART_MATIN = 8;
    constexpr int MINUTE_DEPART_MATIN = 0;
    constexpr int DECALAGE_DEPART_MEME_JOUR_MIN = 90;

    // --- Capacité cabine, plan de sièges interactif et tarification ---
    constexpr int CAPACITE_CABINE_DEFAUT = 120;
    constexpr int NB_PASSAGERS_MIN = 1;
    constexpr int NB_PASSAGERS_MAX = 9;
    constexpr int SIEGES_PAR_RANGEE = 6;
    constexpr int NB_RANGEES_CABINE = 20;
    constexpr int RANGEE_MAX_CLASSE_A = 4;
    constexpr double SUPPLEMENT_SIEGE_CLASSE_A_EUR = 45.0;

    // --- Taux de conversion depuis l'Euro (base interne SQL) ---
    constexpr double TAUX_EUR_VERS_EUR = 1.0;
    constexpr double TAUX_EUR_VERS_MGA = 4950.0;
    constexpr double TAUX_EUR_VERS_USD = 1.08;
    constexpr double TAUX_EUR_VERS_GBP = 0.85;
    constexpr double TAUX_EUR_VERS_MUR = 50.20;

    inline double tauxMonnaieDepuisEur(const QString &codeMonnaie)
    {
        if (codeMonnaie == QStringLiteral("MGA")) return TAUX_EUR_VERS_MGA;
        if (codeMonnaie == QStringLiteral("USD")) return TAUX_EUR_VERS_USD;
        if (codeMonnaie == QStringLiteral("GBP")) return TAUX_EUR_VERS_GBP;
        if (codeMonnaie == QStringLiteral("MUR")) return TAUX_EUR_VERS_MUR;
        return TAUX_EUR_VERS_EUR;
    }

    inline QString symboleMonnaie(const QString &codeMonnaie)
    {
        if (codeMonnaie == QStringLiteral("MGA")) return QStringLiteral("Ar");
        if (codeMonnaie == QStringLiteral("USD")) return QStringLiteral("$");
        if (codeMonnaie == QStringLiteral("GBP")) return QStringLiteral("£");
        if (codeMonnaie == QStringLiteral("MUR")) return QStringLiteral("Rs");
        return QStringLiteral("€");
    }

    inline QString formaterMontant(double montantEur, const QString &codeMonnaie, bool avecSymbole = true)
    {
        double converti = montantEur * tauxMonnaieDepuisEur(codeMonnaie);
        QString nombre;
        if (codeMonnaie == QStringLiteral("MGA")) {
            qint64 entier = qRound64(converti);
            QString brut = QString::number(entier);
            for (int i = brut.size() - 3; i > 0; i -= 3)
                brut.insert(i, QLatin1Char(' '));
            nombre = brut;
        } else {
            nombre = QString::number(converti, 'f', 2);
        }
        return avecSymbole ? QStringLiteral("%1 %2").arg(nombre, symboleMonnaie(codeMonnaie)) : nombre;
    }

    // --- Sécurité des comptes et validations ---
    constexpr int LONGUEUR_MDP_MIN = 6;
    constexpr int AGE_MAX_PASSAGER_ANS = 120;

    // --- Synchronisation multi-instances et défilement du tableau des vols ---
    constexpr int INTERVALLE_SONDAGE_MS = 4000;
    constexpr int HORIZON_PROCHAINS_VOLS_JOURS = 21;
    constexpr int INTERVALLE_DEFILEMENT_VOLS_MS = 55;
    constexpr int TICKS_ROTATION_VOLS_PLEIN_ECRAN = 45;

    // --- Réseau SMTP ---
    constexpr int PORT_SMTP_SSL_DEFAUT = 465;
    constexpr int PORT_SMTP_STARTTLS = 587;
    constexpr int DELAI_TIMEOUT_SMTP_MS = 12000;

    // --- Balise intégrée par le générateur de faux passeport (sans OCR) ---
    inline const QString BALISE_PASSEPORT = QStringLiteral("PASSEPORT_DATA|");

    // --- Domaines d'emails jetables / temporaires explicitement bloqués ---
    inline QStringList domainesEmailsJetables()
    {
        return {
            QStringLiteral("yopmail.com"),
            QStringLiteral("yopmail.fr"),
            QStringLiteral("yopmail.net"),
            QStringLiteral("mailinator.com"),
            QStringLiteral("tempmail.com"),
            QStringLiteral("temp-mail.org"),
            QStringLiteral("guerrillamail.com"),
            QStringLiteral("guerrillamailblock.com"),
            QStringLiteral("sharklasers.com"),
            QStringLiteral("10minutemail.com"),
            QStringLiteral("trashmail.com"),
            QStringLiteral("getnada.com"),
            QStringLiteral("maildrop.cc"),
            QStringLiteral("dispostable.com"),
            QStringLiteral("throwawaymail.com"),
            QStringLiteral("mohmal.com"),
            QStringLiteral("fakeinbox.com"),
            QStringLiteral("jetable.org"),
            QStringLiteral("mytemp.email"),
            QStringLiteral("emailondeck.com"),
            QStringLiteral("mailnesia.com"),
            QStringLiteral("mintemail.com")
        };
    }

} // namespace Constantes

#endif // CONSTANTES_H
