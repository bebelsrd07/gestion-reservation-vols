#include "calculsvol.h"
#include "constantes.h"

#include <QtMath>
#include <QFile>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>

namespace CalculsVol {

int calculerDistanceHaversineKm(double lat1, double lon1, double lat2, double lon2)
{
    double deltaLat = (lat2 - lat1) * Constantes::DEGRES_VERS_RADIANS;
    double deltaLon = (lon2 - lon1) * Constantes::DEGRES_VERS_RADIANS;

    double lat1Rad = lat1 * Constantes::DEGRES_VERS_RADIANS;
    double lat2Rad = lat2 * Constantes::DEGRES_VERS_RADIANS;

    double sinDemiLat = qSin(deltaLat / 2.0);
    double sinDemiLon = qSin(deltaLon / 2.0);

    double a = sinDemiLat * sinDemiLat
               + qCos(lat1Rad) * qCos(lat2Rad) * sinDemiLon * sinDemiLon;
    double c = 2.0 * qAtan2(qSqrt(a), qSqrt(qMax(0.0, 1.0 - a)));

    int distance = qRound(Constantes::RAYON_TERRE_KM * c);
    return qMax(1, distance);
}

int calculerDureeVolMinutes(int distanceKm)
{
    double minutesEnVol = (static_cast<double>(distanceKm) / Constantes::VITESSE_CROISIERE_KMH)
                          * Constantes::MINUTES_PAR_HEURE;
    int totalMinutes = Constantes::FORFAIT_ROULAGE_APPROCHE_MIN + qRound(minutesEnVol);
    return qMax(Constantes::DUREE_VOL_MINIMALE_MIN, totalMinutes);
}

double calculerPrixVolUnitaire(int distanceKm)
{
    double prixBrut = Constantes::PRIX_FIXE_BASE_EUR
                      + static_cast<double>(distanceKm) * Constantes::TARIF_PAR_KM_EUR;
    return qRound(prixBrut * Constantes::FACTEUR_ARRONDI_CENTIMES)
           / Constantes::FACTEUR_ARRONDI_CENTIMES;
}

QString formaterDureeMinutes(int minutes)
{
    int heures = minutes / static_cast<int>(Constantes::MINUTES_PAR_HEURE);
    int resteMinutes = minutes % static_cast<int>(Constantes::MINUTES_PAR_HEURE);
    return QStringLiteral("%1 h %2").arg(heures).arg(resteMinutes, 2, 10, QChar('0'));
}

LiaisonReference calculerReferenceTroncon(
    const AeroportInfo &depart,
    const AeroportInfo &arrivee,
    const QHash<QPair<int, int>, LiaisonReference> &liaisonsConnues)
{
    QPair<int, int> cle(depart.idAeroport, arrivee.idAeroport);
    if (liaisonsConnues.contains(cle)) {
        return liaisonsConnues.value(cle);
    }

    // Si la liaison directe n'est pas pré-enregistrée dans MySQL, je la calcule par Haversine.
    LiaisonReference calc;
    calc.trouvee = true;
    calc.distanceKm = calculerDistanceHaversineKm(
        depart.latitude, depart.longitude, arrivee.latitude, arrivee.longitude);
    calc.dureeMinutes = calculerDureeVolMinutes(calc.distanceKm);
    calc.prixBase = calculerPrixVolUnitaire(calc.distanceKm);
    return calc;
}

static bool correspondVilleEscale(const AeroportInfo &aeroport, const QString &filtreEscale)
{
    if (filtreEscale.trimmed().isEmpty())
        return true;

    QString f = normaliserTexteIdentite(filtreEscale);
    QString complet = normaliserTexteIdentite(aeroport.affichageVillePays());
    QString ville = normaliserTexteIdentite(aeroport.ville);
    QString iata = normaliserTexteIdentite(aeroport.codeIata);

    return complet.contains(f) || f.contains(ville) || f == iata;
}

static ItinerairePropose assemblerItineraire(const QList<TronconVol> &troncons)
{
    ItinerairePropose it;
    it.troncons = troncons;
    if (troncons.isEmpty())
        return it;

    int distanceTotale = 0;
    double prixTotal = 0.0;
    int placesMin = troncons.first().placesDisponibles;

    for (const TronconVol &t : troncons) {
        distanceTotale += t.distanceKm;
        prixTotal += t.prixUnitaire;
        placesMin = qMin(placesMin, t.placesDisponibles);
    }

    it.distanceTotaleKm = distanceTotale;
    it.dureeTotaleMinutes = static_cast<int>(
        troncons.first().dateDepart.secsTo(troncons.last().dateArrivee)
        / static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
    it.prixParPassager = qRound(prixTotal * Constantes::FACTEUR_ARRONDI_CENTIMES)
                         / Constantes::FACTEUR_ARRONDI_CENTIMES;
    it.placesMinDisponibles = placesMin;
    return it;
}

static TronconVol fabriquerTronconCalcule(
    const AeroportInfo &dep,
    const AeroportInfo &arr,
    const QDateTime &dateDepart,
    const QHash<QPair<int, int>, LiaisonReference> &liaisonsConnues)
{
    LiaisonReference ref = calculerReferenceTroncon(dep, arr, liaisonsConnues);
    TronconVol t;
    t.idVolExistant = 0;
    t.numeroVol.clear(); // Généré automatiquement au moment où le client valide la réservation
    t.compagnie = (dep.codePays == QStringLiteral("MG") || arr.codePays == QStringLiteral("MG"))
                      ? QStringLiteral("Madagascar Airlines")
                      : QStringLiteral("Air International");
    t.depart = dep;
    t.arrivee = arr;
    t.dateDepart = dateDepart;
    t.dateArrivee = dateDepart.addSecs(
        static_cast<qint64>(ref.dureeMinutes) * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
    t.distanceKm = ref.distanceKm;
    t.dureeMinutes = ref.dureeMinutes;
    t.prixUnitaire = ref.prixBase;
    t.placesDisponibles = Constantes::CAPACITE_CABINE_DEFAUT;
    return t;
}

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
    const QString &villeEscaleObligatoire)
{
    QList<ItinerairePropose> resultats;
    const QDateTime maintenant = QDateTime::currentDateTime();
    const bool escaleImposee = !villeEscaleObligatoire.trimmed().isEmpty();

    if (filtrerParDate && dateDepartSouhaitee < QDate::currentDate()) {
        return resultats;
    }

    QList<int> idsDep, idsArr;
    for (const AeroportInfo &a : aeroportsDepart)
        idsDep.append(a.idAeroport);
    for (const AeroportInfo &a : aeroportsArrivee)
        idsArr.append(a.idAeroport);

    // 1. Recherche dans les vols déjà programmés en base MySQL (0, 1 ou 2 escales)
    for (const TronconVol &v1 : volsProgrammes) {
        if (v1.dateDepart <= maintenant || v1.placesDisponibles < nbPassagers)
            continue;
        if (!idsDep.isEmpty() && !idsDep.contains(v1.depart.idAeroport))
            continue;
        if (filtrerParDate && v1.dateDepart.date() != dateDepartSouhaitee)
            continue;

        // Cas 0 escale : vol direct programmé
        if (!escaleImposee && (idsArr.isEmpty() || idsArr.contains(v1.arrivee.idAeroport))) {
            resultats.append(assemblerItineraire({v1}));
        }

        // Si l'utilisateur n'a pas précisé départ + arrivée, on n'assemble pas des milliers d'escales
        if (idsDep.isEmpty() || idsArr.isEmpty())
            continue;

        // Cas 1 escale sur vols programmés : v1 -> v2 sans chevauchement horaire
        if (!idsArr.contains(v1.arrivee.idAeroport)) {
            for (const TronconVol &v2 : volsProgrammes) {
                if (v2.placesDisponibles < nbPassagers)
                    continue;
                if (v2.depart.idAeroport != v1.arrivee.idAeroport)
                    continue;
                if (v2.arrivee.ville.compare(v1.depart.ville, Qt::CaseInsensitive) == 0)
                    continue;

                qint64 attente1Min = v1.dateArrivee.secsTo(v2.dateDepart)
                                     / static_cast<qint64>(Constantes::MINUTES_PAR_HEURE);
                if (attente1Min < Constantes::ESCALE_DUREE_MIN_MINUTES
                    || attente1Min > Constantes::ESCALE_DUREE_MAX_MINUTES) {
                    continue;
                }

                if (idsArr.contains(v2.arrivee.idAeroport)) {
                    if (!escaleImposee || correspondVilleEscale(v1.arrivee, villeEscaleObligatoire)) {
                        resultats.append(assemblerItineraire({v1, v2}));
                    }
                    continue;
                }

                // Cas 2 escales sur vols programmés : v1 -> v2 -> v3 sans chevauchement
                for (const TronconVol &v3 : volsProgrammes) {
                    if (v3.placesDisponibles < nbPassagers)
                        continue;
                    if (v3.depart.idAeroport != v2.arrivee.idAeroport)
                        continue;
                    if (!idsArr.contains(v3.arrivee.idAeroport))
                        continue;
                    if (v3.arrivee.ville.compare(v1.arrivee.ville, Qt::CaseInsensitive) == 0)
                        continue;

                    qint64 attente2Min = v2.dateArrivee.secsTo(v3.dateDepart)
                                         / static_cast<qint64>(Constantes::MINUTES_PAR_HEURE);
                    if (attente2Min < Constantes::ESCALE_DUREE_MIN_MINUTES
                        || attente2Min > Constantes::ESCALE_DUREE_MAX_MINUTES) {
                        continue;
                    }

                    if (!escaleImposee
                        || correspondVilleEscale(v1.arrivee, villeEscaleObligatoire)
                        || correspondVilleEscale(v2.arrivee, villeEscaleObligatoire)) {
                        resultats.append(assemblerItineraire({v1, v2, v3}));
                    }
                }
            }
        }
    }

    // 2. Calcul automatique d'itinéraires cohérents (0, 1 et 2 escales) à partir des tables de référence
    if (!aeroportsDepart.isEmpty() && !aeroportsArrivee.isEmpty()) {
        QDate dateRef = filtrerParDate ? dateDepartSouhaitee : QDate::currentDate().addDays(1);
        QDateTime departBase(dateRef, QTime(Constantes::HEURE_DEPART_MATIN,
                                            Constantes::MINUTE_DEPART_MATIN));

        // Je m'assure que l'horaire proposé aujourd'hui est strictement dans le futur
        if (departBase <= maintenant) {
            QDateTime decale = maintenant.addSecs(
                static_cast<qint64>(Constantes::DECALAGE_DEPART_MEME_JOUR_MIN)
                * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
            int minuteArrondie = (decale.time().minute() / 15) * 15;
            departBase = QDateTime(decale.date(), QTime(decale.time().hour(), minuteArrondie));
        }

        if (!filtrerParDate || departBase.date() == dateDepartSouhaitee) {
            for (const AeroportInfo &dep : aeroportsDepart) {
                for (const AeroportInfo &arr : aeroportsArrivee) {
                    if (dep.idAeroport == arr.idAeroport
                        || dep.ville.compare(arr.ville, Qt::CaseInsensitive) == 0) {
                        continue;
                    }

                    int distDirecte = calculerDistanceHaversineKm(
                        dep.latitude, dep.longitude, arr.latitude, arr.longitude);

                    // 2.a Vol direct automatique (si aucune escale obligatoire n'est demandée)
                    bool liaisonDirecteConnue = liaisonsConnues.contains(
                        qMakePair(dep.idAeroport, arr.idAeroport));
                    if (!escaleImposee
                        && (liaisonDirecteConnue
                            || distDirecte <= Constantes::DISTANCE_MAX_VOL_DIRECT_KM)) {
                        TronconVol tDirect = fabriquerTronconCalcule(
                            dep, arr, departBase, liaisonsConnues);
                        resultats.append(assemblerItineraire({tDirect}));
                    }

                    // 2.b Calcul automatique à 1 escale : dep -> escale1 -> arr
                    QList<ItinerairePropose> candidatsUneEscale;
                    for (const AeroportInfo &e1 : tousLesAeroports) {
                        if (e1.idAeroport == dep.idAeroport || e1.idAeroport == arr.idAeroport)
                            continue;
                        if (e1.ville.compare(dep.ville, Qt::CaseInsensitive) == 0
                            || e1.ville.compare(arr.ville, Qt::CaseInsensitive) == 0)
                            continue;

                        if (escaleImposee && !correspondVilleEscale(e1, villeEscaleObligatoire))
                            continue;
                        if (!escaleImposee && !e1.estHub
                            && !liaisonsConnues.contains(qMakePair(dep.idAeroport, e1.idAeroport)))
                            continue;

                        TronconVol t1 = fabriquerTronconCalcule(dep, e1, departBase, liaisonsConnues);
                        QDateTime depT2 = t1.dateArrivee.addSecs(
                            static_cast<qint64>(Constantes::ESCALE_DUREE_STANDARD_MINUTES)
                            * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
                        TronconVol t2 = fabriquerTronconCalcule(e1, arr, depT2, liaisonsConnues);

                        if (t1.distanceKm > Constantes::DISTANCE_MAX_VOL_DIRECT_KM
                            || t2.distanceKm > Constantes::DISTANCE_MAX_VOL_DIRECT_KM) {
                            continue;
                        }

                        double ratioDetour = static_cast<double>(t1.distanceKm + t2.distanceKm)
                                             / static_cast<double>(qMax(1, distDirecte));
                        if (!escaleImposee && ratioDetour > Constantes::RATIO_DETOUR_MAX_UNE_ESCALE)
                            continue;

                        candidatsUneEscale.append(assemblerItineraire({t1, t2}));
                    }

                    std::sort(candidatsUneEscale.begin(), candidatsUneEscale.end(),
                              [](const ItinerairePropose &a, const ItinerairePropose &b) {
                                  return a.dureeTotaleMinutes < b.dureeTotaleMinutes;
                              });
                    for (int i = 0; i < qMin(Constantes::MAX_ITINERAIRES_CALCULES_PAR_TYPE,
                                             candidatsUneEscale.size()); ++i) {
                        resultats.append(candidatsUneEscale[i]);
                    }

                    // 2.c Calcul automatique à 2 escales : dep -> escale1 -> escale2 -> arr
                    QList<ItinerairePropose> candidatsDeuxEscales;
                    for (const AeroportInfo &e1 : tousLesAeroports) {
                        if (!e1.estHub && !(escaleImposee && correspondVilleEscale(e1, villeEscaleObligatoire)))
                            continue;
                        if (e1.ville.compare(dep.ville, Qt::CaseInsensitive) == 0
                            || e1.ville.compare(arr.ville, Qt::CaseInsensitive) == 0)
                            continue;

                        TronconVol t1 = fabriquerTronconCalcule(dep, e1, departBase, liaisonsConnues);
                        if (t1.distanceKm > Constantes::DISTANCE_MAX_VOL_DIRECT_KM)
                            continue;

                        for (const AeroportInfo &e2 : tousLesAeroports) {
                            if (!e2.estHub && !(escaleImposee && correspondVilleEscale(e2, villeEscaleObligatoire)))
                                continue;
                            if (e2.ville.compare(dep.ville, Qt::CaseInsensitive) == 0
                                || e2.ville.compare(arr.ville, Qt::CaseInsensitive) == 0
                                || e2.ville.compare(e1.ville, Qt::CaseInsensitive) == 0)
                                continue;

                            if (escaleImposee
                                && !correspondVilleEscale(e1, villeEscaleObligatoire)
                                && !correspondVilleEscale(e2, villeEscaleObligatoire)) {
                                continue;
                            }

                            QDateTime depT2 = t1.dateArrivee.addSecs(
                                static_cast<qint64>(Constantes::ESCALE_DUREE_STANDARD_MINUTES)
                                * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
                            TronconVol t2 = fabriquerTronconCalcule(e1, e2, depT2, liaisonsConnues);
                            if (t2.distanceKm > Constantes::DISTANCE_MAX_VOL_DIRECT_KM)
                                continue;

                            QDateTime depT3 = t2.dateArrivee.addSecs(
                                static_cast<qint64>(Constantes::ESCALE_DUREE_STANDARD_MINUTES)
                                * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
                            TronconVol t3 = fabriquerTronconCalcule(e2, arr, depT3, liaisonsConnues);
                            if (t3.distanceKm > Constantes::DISTANCE_MAX_VOL_DIRECT_KM)
                                continue;

                            double ratioDetour = static_cast<double>(
                                                     t1.distanceKm + t2.distanceKm + t3.distanceKm)
                                                 / static_cast<double>(qMax(1, distDirecte));
                            if (!escaleImposee
                                && ratioDetour > Constantes::RATIO_DETOUR_MAX_DEUX_ESCALES) {
                                continue;
                            }

                            candidatsDeuxEscales.append(assemblerItineraire({t1, t2, t3}));
                        }
                    }

                    std::sort(candidatsDeuxEscales.begin(), candidatsDeuxEscales.end(),
                              [](const ItinerairePropose &a, const ItinerairePropose &b) {
                                  return a.dureeTotaleMinutes < b.dureeTotaleMinutes;
                              });
                    for (int i = 0; i < qMin(Constantes::MAX_ITINERAIRES_CALCULES_PAR_TYPE - 1,
                                             candidatsDeuxEscales.size()); ++i) {
                        resultats.append(candidatsDeuxEscales[i]);
                    }
                }
            }
        }
    }

    // 3. Filtrage par prix maximum et dédoublonnage propre
    QList<ItinerairePropose> filtres;
    QSet<QString> signaturesVues;

    for (const ItinerairePropose &it : resultats) {
        if (filtrerPrixMax && it.prixParPassager > prixMaxParPassager)
            continue;

        QString cle = it.resumeTrajet() + QStringLiteral("|")
                      + it.dateDepartInitiale().toString(Qt::ISODate);
        if (signaturesVues.contains(cle))
            continue;
        signaturesVues.insert(cle);
        filtres.append(it);
    }

    std::sort(filtres.begin(), filtres.end(),
              [](const ItinerairePropose &a, const ItinerairePropose &b) {
                  if (!qFuzzyCompare(a.prixParPassager, b.prixParPassager))
                      return a.prixParPassager < b.prixParPassager;
                  return a.dureeTotaleMinutes < b.dureeTotaleMinutes;
              });

    return filtres;
}

bool validerEmailStrict(const QString &email, QString &messageErreur)
{
    QString propre = email.trimmed().toLower();
    if (propre.isEmpty()) {
        messageErreur = QStringLiteral("Veuillez saisir une adresse email.");
        return false;
    }

    // Interdiction explicite des alias contenant le caractère '+'
    if (propre.contains(QLatin1Char('+'))) {
        messageErreur = QStringLiteral(
            "Les alias email contenant le symbole '+' ne sont pas autorisés.");
        return false;
    }

    static const QRegularExpression motifEmail(
        QStringLiteral("^[a-zA-Z0-9._-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,10}$"));
    if (!motifEmail.match(propre).hasMatch()) {
        messageErreur = QStringLiteral("Le format de l'adresse email est invalide.");
        return false;
    }

    QString domaine = propre.section(QLatin1Char('@'), 1);
    const QStringList jetables = Constantes::domainesEmailsJetables();
    for (const QString &interdit : jetables) {
        if (domaine == interdit || domaine.endsWith(QLatin1Char('.') + interdit)) {
            messageErreur = QStringLiteral(
                "Les adresses email temporaires ou jetables (%1) sont interdites.")
                                .arg(domaine);
            return false;
        }
    }

    messageErreur.clear();
    return true;
}

QString normaliserTexteIdentite(const QString &texte)
{
    // Je décompose les accents (NFD), je retire les marques diacritiques et je passe en majuscules
    QString decomp = texte.trimmed().normalized(QString::NormalizationForm_D);
    QString resultat;
    resultat.reserve(decomp.size());

    for (const QChar &c : decomp) {
        if (c.category() != QChar::Mark_NonSpacing) {
            resultat.append(c);
        }
    }

    static const QRegularExpression espacesMultiples(QStringLiteral("\\s+"));
    return resultat.replace(espacesMultiples, QStringLiteral(" ")).trimmed().toUpper();
}

bool lireFichierPasseportSansOcr(const QString &cheminFichier,
                                 QString &numeroPasseport,
                                 QString &nom,
                                 QString &prenom,
                                 QString &messageErreur)
{
    QFile fichier(cheminFichier);
    if (!fichier.open(QIODevice::ReadOnly)) {
        messageErreur = QStringLiteral("Impossible d'ouvrir le fichier sélectionné.");
        return false;
    }

    QByteArray contenu = fichier.readAll();
    fichier.close();

    QByteArray marqueur = Constantes::BALISE_PASSEPORT.toUtf8();
    int debut = contenu.indexOf(marqueur);
    if (debut < 0) {
        messageErreur = QStringLiteral(
            "Ce fichier ne contient pas les métadonnées attendues. "
            "Veuillez générer le passeport avec l'outil generateur_passeport/index.html.");
        return false;
    }

    int longueurMax = qMin(400, contenu.size() - debut);
    QString extrait = QString::fromUtf8(contenu.mid(debut, longueurMax));
    // En PDF, les parenthèses échappées éventuelles sont nettoyées
    extrait.replace(QStringLiteral("\\("), QStringLiteral("("));
    extrait.replace(QStringLiteral("\\)"), QStringLiteral(")"));

    const QStringList blocs = extrait.split(QLatin1Char('|'));
    numeroPasseport.clear();
    nom.clear();
    prenom.clear();

    for (const QString &bloc : blocs) {
        if (bloc.startsWith(QStringLiteral("NUMERO="))) {
            numeroPasseport = bloc.mid(7).trimmed().toUpper();
        } else if (bloc.startsWith(QStringLiteral("NOM="))) {
            nom = bloc.mid(4).trimmed();
        } else if (bloc.startsWith(QStringLiteral("PRENOM="))) {
            prenom = bloc.mid(7).trimmed();
        }
    }

    if (numeroPasseport.isEmpty() || nom.isEmpty() || prenom.isEmpty()) {
        messageErreur = QStringLiteral("Les données du passeport importé sont incomplètes.");
        return false;
    }

    messageErreur.clear();
    return true;
}

} // namespace CalculsVol
