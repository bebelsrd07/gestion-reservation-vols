#include "billetpdf.h"
#include "calculsvol.h"

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

namespace BilletPdf {

namespace MiseEnPage {
constexpr int RESOLUTION_DPI = 300;
constexpr int MARGE_GAUCHE = 140;
constexpr int MARGE_DROITE = 140;
constexpr int MARGE_HAUT = 130;
constexpr int MARGE_BAS = 160;
constexpr int HAUTEUR_BANDEAU = 255;
constexpr int HAUTEUR_LIGNE_VOL = 140;
constexpr int HAUTEUR_LIGNE_PASSAGER = 95;
}

QString cheminParDefautBillet(const QString &reference)
{
    QDir dossier(QDir::currentPath() + QStringLiteral("/billets_generes"));
    if (!dossier.exists())
        dossier.mkpath(QStringLiteral("."));
    return dossier.filePath(QStringLiteral("Billet_%1.pdf").arg(reference));
}

bool genererBilletUnique(const DetailsReservation &reservation,
                         const QString &cheminFichierPdf,
                         QString &erreur)
{
    if (reservation.reference.isEmpty() || reservation.troncons.isEmpty()) {
        erreur = QStringLiteral("Données de réservation incomplètes pour générer le billet PDF.");
        return false;
    }

    QFileInfo info(cheminFichierPdf);
    if (!info.dir().exists())
        info.dir().mkpath(QStringLiteral("."));

    QPdfWriter pdf(cheminFichierPdf);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setResolution(MiseEnPage::RESOLUTION_DPI);
    pdf.setTitle(QStringLiteral("Billet TicketFly ") + reservation.reference);

    QPainter peintre(&pdf);
    if (!peintre.isActive()) {
        erreur = QStringLiteral("Impossible de créer le document PDF à l'emplacement choisi.");
        return false;
    }

    // Palette officielle TicketFly (style Gumroad / Happy Hues n°10)
    const QColor vertProfond(0x00, 0x46, 0x43); // #004643
    const QColor encre(0x00, 0x1E, 0x1D);       // #001e1d
    const QColor menthe(0xAB, 0xD1, 0xC6);      // #abd1c6
    const QColor mentheClaire(0xEE, 0xF6, 0xF3);
    const QColor jauneAccent(0xF9, 0xBC, 0x60); // #f9bc60
    const QColor ivoire(0xFF, 0xFF, 0xFE);      // #fffffe

    const int largeurPage = pdf.width();
    const int hauteurPage = pdf.height();
    const int largeurUtile = largeurPage - MiseEnPage::MARGE_GAUCHE - MiseEnPage::MARGE_DROITE;

    QFont policeTitre(QStringLiteral("Sans Serif"), 17, QFont::Bold);
    QFont policeSousTitre(QStringLiteral("Sans Serif"), 11, QFont::Bold);
    QFont policeNormale(QStringLiteral("Sans Serif"), 9);
    QFont policeGras(QStringLiteral("Sans Serif"), 9, QFont::Bold);
    QFont policePetite(QStringLiteral("Sans Serif"), 8);

    int y = MiseEnPage::MARGE_HAUT;

    // 1. Bandeau supérieur TicketFly (ombre dure Néo-Brutaliste + vert profond + badge jaune)
    QRect rectBandeau(MiseEnPage::MARGE_GAUCHE, y, largeurUtile, MiseEnPage::HAUTEUR_BANDEAU);
    peintre.setPen(Qt::NoPen);
    peintre.setBrush(encre);
    peintre.drawRoundedRect(rectBandeau.translated(10, 10), 20, 20);

    peintre.setPen(QPen(encre, 5));
    peintre.setBrush(vertProfond);
    peintre.drawRoundedRect(rectBandeau, 20, 20);

    QRect rectBadge(MiseEnPage::MARGE_GAUCHE + largeurUtile - 450, y + 36, 400, 72);
    peintre.setPen(QPen(encre, 4));
    peintre.setBrush(jauneAccent);
    peintre.drawRoundedRect(rectBadge, 12, 12);
    peintre.setPen(encre);
    peintre.setFont(policeSousTitre);
    peintre.drawText(rectBadge, Qt::AlignCenter, QStringLiteral("✈  TICKETFLY"));

    peintre.setPen(ivoire);
    peintre.setFont(policeTitre);
    peintre.drawText(QRect(MiseEnPage::MARGE_GAUCHE + 50, y + 32, largeurUtile - 520, 95),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("BILLET D'AVION ÉLECTRONIQUE"));

    peintre.setPen(menthe);
    peintre.setFont(policeNormale);
    QString statutAffiche = (reservation.statut == QStringLiteral("confirmee"))
                                ? QStringLiteral("CONFIRMÉE")
                                : reservation.statut.toUpper();
    peintre.drawText(QRect(MiseEnPage::MARGE_GAUCHE + 50, y + 132, largeurUtile - 100, 95),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Référence : %1   |   Statut : %2   |   Émis le : %3")
                         .arg(reservation.reference,
                              statutAffiche,
                              QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))));

    y += MiseEnPage::HAUTEUR_BANDEAU + 65;

    // 2. Carte récapitulative du titulaire & paiement TicketFly
    peintre.setPen(QPen(encre, 4));
    peintre.setBrush(mentheClaire);
    peintre.drawRoundedRect(MiseEnPage::MARGE_GAUCHE, y, largeurUtile, 210, 14, 14);

    peintre.setPen(encre);
    peintre.setFont(policeGras);
    peintre.drawText(MiseEnPage::MARGE_GAUCHE + 40, y + 58,
                     QStringLiteral("Client titulaire : %1 %2 (%3)")
                         .arg(reservation.prenomClient,
                              reservation.nomClient,
                              reservation.emailClient));

    peintre.setFont(policeNormale);
    peintre.drawText(MiseEnPage::MARGE_GAUCHE + 40, y + 120,
                     QStringLiteral("Passagers : %1   |   Escales : %2   |   Montant total : %3 EUR")
                         .arg(reservation.passagers.size())
                         .arg(reservation.nbEscales)
                         .arg(QString::number(reservation.prixTotal, 'f', 2)));

    QString infoPaiement = reservation.referencePaiement.trimmed().isEmpty()
                               ? QStringLiteral("Paiement enregistré auprès de l'agence TicketFly")
                               : QStringLiteral("Paiement TicketFly : %1 — %2 (Réf : %3)")
                                     .arg(reservation.modePaiement, reservation.contactPaiement, reservation.referencePaiement);
    peintre.setPen(vertProfond);
    peintre.drawText(MiseEnPage::MARGE_GAUCHE + 40, y + 178, infoPaiement);

    y += 265;

    // 3. Détail chronologique des vols
    peintre.setPen(vertProfond);
    peintre.setFont(policeSousTitre);
    peintre.drawText(MiseEnPage::MARGE_GAUCHE, y, QStringLiteral("ITINÉRAIRE DE VOL DÉTAILLÉ"));
    y += 35;

    for (int i = 0; i < reservation.troncons.size(); ++i) {
        const TronconVol &t = reservation.troncons[i];

        peintre.setPen(QPen(encre, 3));
        peintre.setBrush(ivoire);
        peintre.drawRoundedRect(MiseEnPage::MARGE_GAUCHE, y,
                                largeurUtile, MiseEnPage::HAUTEUR_LIGNE_VOL, 12, 12);

        // Bande latérale jaune Gumroad à gauche de chaque tronçon
        peintre.setPen(Qt::NoPen);
        peintre.setBrush(jauneAccent);
        peintre.drawRoundedRect(MiseEnPage::MARGE_GAUCHE + 6, y + 6,
                                18, MiseEnPage::HAUTEUR_LIGNE_VOL - 12, 6, 6);

        peintre.setPen(vertProfond);
        peintre.setFont(policeGras);
        peintre.drawText(MiseEnPage::MARGE_GAUCHE + 45, y + 54,
                         QStringLiteral("Tronçon %1 — Vol %2 (%3)")
                             .arg(i + 1)
                             .arg(t.numeroVol, t.compagnie));

        peintre.setPen(encre);
        peintre.setFont(policeNormale);
        QString ligneTrajet = QStringLiteral(
                                  "Départ : %1 (%2) le %3   →   Arrivée : %4 (%5) le %6   |   %7 km (%8)")
                                  .arg(t.depart.ville,
                                       t.depart.codeIata,
                                       t.dateDepart.toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                                       t.arrivee.ville,
                                       t.arrivee.codeIata,
                                       t.dateArrivee.toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                                       QString::number(t.distanceKm),
                                       CalculsVol::formaterDureeMinutes(t.dureeMinutes));
        peintre.drawText(MiseEnPage::MARGE_GAUCHE + 45, y + 108, ligneTrajet);

        y += MiseEnPage::HAUTEUR_LIGNE_VOL + 22;

        if (i < reservation.troncons.size() - 1) {
            int escaleMin = static_cast<int>(
                t.dateArrivee.secsTo(reservation.troncons[i + 1].dateDepart) / 60);
            peintre.setPen(vertProfond);
            peintre.setFont(policePetite);
            peintre.drawText(MiseEnPage::MARGE_GAUCHE + 45, y + 18,
                             QStringLiteral("Correspondance à %1 (%2) — temps d'escale : %3")
                                 .arg(t.arrivee.ville,
                                      t.arrivee.codeIata,
                                      CalculsVol::formaterDureeMinutes(escaleMin)));
            y += 42;
        }
    }

    y += 45;

    // 4. Tableau des passagers aux couleurs TicketFly
    peintre.setPen(vertProfond);
    peintre.setFont(policeSousTitre);
    peintre.drawText(MiseEnPage::MARGE_GAUCHE, y,
                     QStringLiteral("PASSAGERS COUVERTS PAR CE BILLET"));
    y += 35;

    const int colNum = MiseEnPage::MARGE_GAUCHE + 25;
    const int colIdentite = MiseEnPage::MARGE_GAUCHE + 160;
    const int colNaissance = MiseEnPage::MARGE_GAUCHE + 980;
    const int colPasseport = MiseEnPage::MARGE_GAUCHE + 1430;
    const int colSiege = MiseEnPage::MARGE_GAUCHE + 1900;

    peintre.setPen(QPen(encre, 3));
    peintre.setBrush(vertProfond);
    peintre.drawRect(MiseEnPage::MARGE_GAUCHE, y, largeurUtile, 85);

    peintre.setPen(ivoire);
    peintre.setFont(policeGras);
    peintre.drawText(colNum, y + 55, QStringLiteral("N°"));
    peintre.drawText(colIdentite, y + 55, QStringLiteral("Nom et Prénom(s)"));
    peintre.drawText(colNaissance, y + 55, QStringLiteral("Date de naissance"));
    peintre.drawText(colPasseport, y + 55, QStringLiteral("N° Passeport"));
    peintre.drawText(colSiege, y + 55, QStringLiteral("Siège"));
    y += 85;

    peintre.setFont(policeNormale);
    for (int i = 0; i < reservation.passagers.size(); ++i) {
        if (y + MiseEnPage::HAUTEUR_LIGNE_PASSAGER > hauteurPage - MiseEnPage::MARGE_BAS) {
            pdf.newPage();
            y = MiseEnPage::MARGE_HAUT;
        }

        const PassagerInfo &p = reservation.passagers[i];
        peintre.setPen(QPen(encre, 2));
        peintre.setBrush((i % 2 == 0) ? ivoire : mentheClaire);
        peintre.drawRect(MiseEnPage::MARGE_GAUCHE, y,
                         largeurUtile, MiseEnPage::HAUTEUR_LIGNE_PASSAGER);

        peintre.setPen(encre);
        peintre.drawText(colNum, y + 60, QString::number(i + 1));
        peintre.drawText(colIdentite, y + 60,
                         QStringLiteral("%1 %2").arg(p.nom.toUpper(), p.prenom));
        peintre.drawText(colNaissance, y + 60,
                         p.dateNaissance.toString(QStringLiteral("dd/MM/yyyy")));
        peintre.drawText(colPasseport, y + 60, p.numeroPasseport);

        peintre.setFont(policeGras);
        peintre.setPen(vertProfond);
        peintre.drawText(colSiege, y + 60, p.siege);
        peintre.setFont(policeNormale);

        y += MiseEnPage::HAUTEUR_LIGNE_PASSAGER;
    }

    y += 55;
    QRect rectPied(MiseEnPage::MARGE_GAUCHE, y, largeurUtile, 115);
    peintre.setPen(QPen(encre, 3));
    peintre.setBrush(jauneAccent);
    peintre.drawRoundedRect(rectPied, 12, 12);

    peintre.setPen(encre);
    peintre.setFont(policePetite);
    peintre.drawText(rectPied.adjusted(30, 12, -30, -12),
                     Qt::AlignVCenter | Qt::TextWordWrap,
                     QStringLiteral("TICKETFLY — Présentez ce billet électronique ainsi que le passeport individuel de chaque passager "
                                    "lors de l'enregistrement à l'aéroport au moins 2 heures avant le départ."));

    peintre.end();
    erreur.clear();
    return true;
}

} // namespace BilletPdf