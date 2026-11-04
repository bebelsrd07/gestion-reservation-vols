#include "planavionwidget.h"
#include "constantes.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace Cabine {
    constexpr int LARGEUR_WIDGET = 430;
    constexpr int LARGEUR_FUSELAGE = 252;
    constexpr int HAUTEUR_NEZ = 88;
    constexpr int LARGEUR_SIEGE = 30;
    constexpr int HAUTEUR_SIEGE = 26;
    constexpr int ESPACE_X = 5;
    constexpr int LARGEUR_COULOIR = 34;
    constexpr int PAS_RANGEE_Y = 31;
    constexpr int HAUTEUR_WC = 30;
    constexpr int HAUTEUR_SEPARATION = 30;
    constexpr int HAUTEUR_QUEUE = 86;
}

PlanAvionWidget::PlanAvionWidget(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    const int hauteurTotale = Cabine::HAUTEUR_NEZ
                              + Cabine::HAUTEUR_WC + 38
                              + Constantes::NB_RANGEES_CABINE * Cabine::PAS_RANGEE_Y
                              + Cabine::HAUTEUR_SEPARATION
                              + Cabine::HAUTEUR_WC
                              + Cabine::HAUTEUR_QUEUE;
    setMinimumSize(Cabine::LARGEUR_WIDGET, hauteurTotale);
}

void PlanAvionWidget::configurerCabine(int nbPassagersRequis, const QSet<QString> &siegesOccupes)
{
    m_nbPassagers = qMax(1, nbPassagersRequis);
    m_occupes = siegesOccupes;
    m_siegesChoisis.clear();
    update();
    emit selectionModifiee();
}

void PlanAvionWidget::definirMonnaie(const QString &codeMonnaie)
{
    m_codeMonnaie = codeMonnaie;
    update();
}

bool PlanAvionWidget::estSiegeClasseA(const QString &codeSiege)
{
    QString chiffres;
    for (const QChar &c : codeSiege) {
        if (c.isDigit())
            chiffres.append(c);
        else
            break;
    }
    int rangee = chiffres.toInt();
    return rangee >= 1 && rangee <= Constantes::RANGEE_MAX_CLASSE_A;
}

QString PlanAvionWidget::formaterLibelleSiege(const QString &codeSiege, const QString &codeMonnaie)
{
    if (codeSiege.isEmpty())
        return QStringLiteral("À choisir sur l'avion");
    if (estSiegeClasseA(codeSiege)) {
        return QStringLiteral("%1 — Classe A (+%2)")
            .arg(codeSiege,
                 Constantes::formaterMontant(Constantes::SUPPLEMENT_SIEGE_CLASSE_A_EUR, codeMonnaie));
    }
    return QStringLiteral("%1 — Régulier").arg(codeSiege);
}

double PlanAvionWidget::calculerSupplementTotal() const
{
    int nbClasseA = 0;
    for (const QString &code : m_siegesChoisis) {
        if (estSiegeClasseA(code))
            ++nbClasseA;
    }
    return nbClasseA * Constantes::SUPPLEMENT_SIEGE_CLASSE_A_EUR;
}

static void dessinerZoneService(QPainter &p, int xFus, int largFus, int y, const QString &droite)
{
    const QColor trait(0, 30, 29);
    const int marge = 14;
    const int larg = (largFus - 3 * marge) / 2;
    QRect rGauche(xFus + marge, y, larg, 23);
    QRect rDroite(xFus + 2 * marge + larg, y, larg, 23);

    p.setPen(QPen(trait, 1.6));
    p.setBrush(QColor(232, 228, 230));
    p.drawRoundedRect(rGauche, 6, 6);
    p.drawRoundedRect(rDroite, 6, 6);
    p.drawText(rGauche, Qt::AlignCenter, QStringLiteral("WC"));
    p.drawText(rDroite, Qt::AlignCenter, droite);
}

void PlanAvionWidget::paintEvent(QPaintEvent *)
{
    m_zonesSieges.clear();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor trait(0, 30, 29);        // #001e1d
    const QColor vertProfond(0, 70, 67);  // #004643
    const QColor vertEau(171, 209, 198);  // #abd1c6
    const QColor jauneOr(249, 188, 96);   // #f9bc60
    const QColor ivoire(255, 255, 254);   // #fffffe
    const QColor vertChoisi(21, 128, 61); // #15803d

    const int cx = width() / 2;
    const int xFus = cx - Cabine::LARGEUR_FUSELAGE / 2;
    const int dFus = xFus + Cabine::LARGEUR_FUSELAGE;
    const int yDebutQueue = height() - Cabine::HAUTEUR_QUEUE;

    QFont fPetite = font();
    fPetite.setPointSize(8);
    QFont fGras = fPetite;
    fGras.setBold(true);

    // 1. Grandes ailes en flèche + réacteurs sous les ailes (silhouette d'avion immédiate)
    const int yAile = Cabine::HAUTEUR_NEZ + 175;
    p.setPen(QPen(trait, 2));
    p.setBrush(vertProfond);

    // Réacteurs (nacelles moteurs) gauche et droite
    p.drawRoundedRect(QRect(xFus - 46, yAile + 20, 22, 44), 8, 8);
    p.drawRoundedRect(QRect(dFus + 24, yAile + 20, 22, 44), 8, 8);

    // Ailes principales en flèche
    p.setBrush(QColor(214, 232, 226));
    QPolygon aileGauche;
    aileGauche << QPoint(xFus, yAile)
               << QPoint(6, yAile + 85)
               << QPoint(6, yAile + 118)
               << QPoint(xFus, yAile + 92);
    QPolygon aileDroite;
    aileDroite << QPoint(dFus, yAile)
               << QPoint(width() - 6, yAile + 85)
               << QPoint(width() - 6, yAile + 118)
               << QPoint(dFus, yAile + 92);
    p.drawPolygon(aileGauche);
    p.drawPolygon(aileDroite);

    // Empennage arrière (stabilisateurs horizontaux à la queue)
    QPolygon stabGauche;
    stabGauche << QPoint(xFus + 16, yDebutQueue + 10)
               << QPoint(xFus - 52, yDebutQueue + 54)
               << QPoint(xFus - 52, yDebutQueue + 72)
               << QPoint(xFus + 42, yDebutQueue + 56);
    QPolygon stabDroite;
    stabDroite << QPoint(dFus - 16, yDebutQueue + 10)
               << QPoint(dFus + 52, yDebutQueue + 54)
               << QPoint(dFus + 52, yDebutQueue + 72)
               << QPoint(dFus - 42, yDebutQueue + 56);
    p.drawPolygon(stabGauche);
    p.drawPolygon(stabDroite);

    // 2. Carlingue aérodynamique (nez pointu à l'avant + cône de queue effilé à l'arrière)
    QPainterPath carlingue;
    carlingue.moveTo(xFus, Cabine::HAUTEUR_NEZ);
    carlingue.cubicTo(xFus, 34, cx - 34, 8, cx, 8);
    carlingue.cubicTo(cx + 34, 8, dFus, 34, dFus, Cabine::HAUTEUR_NEZ);
    carlingue.lineTo(dFus, yDebutQueue);
    carlingue.cubicTo(dFus, yDebutQueue + 38, cx + 22, height() - 8, cx, height() - 6);
    carlingue.cubicTo(cx - 22, height() - 8, xFus, yDebutQueue + 38, xFus, yDebutQueue);
    carlingue.closeSubpath();

    p.setPen(QPen(trait, 2.5));
    p.setBrush(ivoire);
    p.drawPath(carlingue);

    // Dérive centrale sur la queue
    p.setPen(QPen(vertProfond, 4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(cx, yDebutQueue + 20, cx, height() - 10);

    // Verrière double du cockpit
    p.setPen(QPen(trait, 2));
    p.setBrush(vertProfond);
    QPainterPath cockpitG, cockpitD;
    cockpitG.moveTo(cx - 4, 30);
    cockpitG.quadTo(cx - 34, 34, cx - 44, 54);
    cockpitG.lineTo(cx - 4, 48);
    cockpitG.closeSubpath();
    cockpitD.moveTo(cx + 4, 30);
    cockpitD.quadTo(cx + 34, 34, cx + 44, 54);
    cockpitD.lineTo(cx + 4, 48);
    cockpitD.closeSubpath();
    p.drawPath(cockpitG);
    p.drawPath(cockpitD);

    // 3. Intérieur cabine : WC avant + en-têtes
    int y = Cabine::HAUTEUR_NEZ + 2;
    p.setFont(fGras);
    dessinerZoneService(p, xFus, Cabine::LARGEUR_FUSELAGE, y, QStringLiteral("GALLEY"));
    y += Cabine::HAUTEUR_WC;

    // Bandeau Classe A
    QRect bandeauA(xFus + 14, y, Cabine::LARGEUR_FUSELAGE - 28, 19);
    p.setPen(QPen(trait, 1.5));
    p.setBrush(jauneOr);
    p.drawRoundedRect(bandeauA, 5, 5);
    p.drawText(bandeauA, Qt::AlignCenter,
               QStringLiteral("★ CLASSE A (+%1)")
                   .arg(Constantes::formaterMontant(Constantes::SUPPLEMENT_SIEGE_CLASSE_A_EUR,
                                                    m_codeMonnaie)));
    y += 22;

    static const char colonnes[] = "ABCDEF";
    const int largBloc3 = 3 * Cabine::LARGEUR_SIEGE + 2 * Cabine::ESPACE_X;
    const int xGauche = cx - (2 * largBloc3 + Cabine::LARGEUR_COULOIR) / 2;
    const int xDroite = xGauche + largBloc3 + Cabine::LARGEUR_COULOIR;

    p.setPen(vertProfond);
    for (int c = 0; c < Constantes::SIEGES_PAR_RANGEE; ++c) {
        int xCol = (c < 3) ? xGauche + c * (Cabine::LARGEUR_SIEGE + Cabine::ESPACE_X)
                           : xDroite + (c - 3) * (Cabine::LARGEUR_SIEGE + Cabine::ESPACE_X);
        p.drawText(QRect(xCol, y, Cabine::LARGEUR_SIEGE, 15),
                   Qt::AlignCenter, QString(QLatin1Char(colonnes[c])));
    }
    y += 17;

    // Tapis de couloir central discret
    const int hauteurToutesRangees = Constantes::NB_RANGEES_CABINE * Cabine::PAS_RANGEE_Y
                                     + Cabine::HAUTEUR_SEPARATION;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(238, 245, 242));
    p.drawRoundedRect(QRect(xGauche + largBloc3 + 4, y, Cabine::LARGEUR_COULOIR - 8,
                            hauteurToutesRangees - 6), 4, 4);

    // 4. Dessin des fauteuils d'avion (rangées 1 à 20) + hublots latéraux
    for (int rangee = 1; rangee <= Constantes::NB_RANGEES_CABINE; ++rangee) {
        if (rangee == Constantes::RANGEE_MAX_CLASSE_A + 1) {
            QRect bandeauReg(xFus + 14, y + 3, Cabine::LARGEUR_FUSELAGE - 28, 21);
            p.setPen(QPen(trait, 1.5));
            p.setBrush(vertEau);
            p.drawRoundedRect(bandeauReg, 5, 5);
            p.setFont(fGras);
            p.drawText(bandeauReg, Qt::AlignCenter,
                       QStringLiteral("◀ EXIT  •  CABINE RÉGULIÈRE (+0 %1)  •  EXIT ▶")
                           .arg(Constantes::symboleMonnaie(m_codeMonnaie)));
            y += Cabine::HAUTEUR_SEPARATION;
        }

        const bool classeA = (rangee <= Constantes::RANGEE_MAX_CLASSE_A);

        // Hublots ovales sur les parois gauche et droite de l'avion
        p.setPen(QPen(vertProfond, 1));
        p.setBrush(vertEau);
        p.drawRoundedRect(QRect(xFus + 4, y + 6, 4, 12), 2, 2);
        p.drawRoundedRect(QRect(dFus - 8, y + 6, 4, 12), 2, 2);

        // Numéro de rangée au centre du couloir
        p.setFont(fPetite);
        p.setPen(vertProfond);
        p.drawText(QRect(xGauche + largBloc3, y, Cabine::LARGEUR_COULOIR, Cabine::HAUTEUR_SIEGE),
                   Qt::AlignCenter, QString::number(rangee));

        for (int c = 0; c < Constantes::SIEGES_PAR_RANGEE; ++c) {
            int xS = (c < 3) ? xGauche + c * (Cabine::LARGEUR_SIEGE + Cabine::ESPACE_X)
                             : xDroite + (c - 3) * (Cabine::LARGEUR_SIEGE + Cabine::ESPACE_X);

            QRect rectSiege(xS, y, Cabine::LARGEUR_SIEGE, Cabine::HAUTEUR_SIEGE);
            QString code = QStringLiteral("%1%2").arg(rangee).arg(QLatin1Char(colonnes[c]));
            m_zonesSieges.insert(code, rectSiege);

            const bool estOccupe = m_occupes.contains(code);
            const int idxPax = m_siegesChoisis.indexOf(code);

            QColor fond, bord, couleurTexte;
            QString etiquette;

            if (estOccupe) {
                fond = QColor(214, 211, 209);
                bord = QColor(140, 145, 145);
                couleurTexte = QColor(115, 120, 120);
                etiquette = QStringLiteral("•");
            } else if (idxPax >= 0) {
                fond = vertChoisi;
                bord = trait;
                couleurTexte = ivoire;
                etiquette = QStringLiteral("P%1").arg(idxPax + 1);
            } else if (classeA) {
                fond = jauneOr;
                bord = trait;
                couleurTexte = trait;
                etiquette = QString(QLatin1Char(colonnes[c]));
            } else {
                fond = ivoire;
                bord = trait;
                couleurTexte = trait;
                etiquette = QString(QLatin1Char(colonnes[c]));
            }

            // Assise du fauteuil + dossier arrondi en bas (tourné vers le cockpit)
            p.setPen(QPen(bord, idxPax >= 0 ? 2.0 : 1.5));
            p.setBrush(fond);
            QRect assise = rectSiege.adjusted(2, 1, -2, -3);
            p.drawRoundedRect(assise, 5, 5);

            // Accoudoirs gauche/droite + appuie-tête en bas du fauteuil
            p.setBrush(bord.lighter(estOccupe ? 115 : 140));
            p.drawRoundedRect(QRect(rectSiege.left(), rectSiege.top() + 6, 3, 15), 1, 1);
            p.drawRoundedRect(QRect(rectSiege.right() - 2, rectSiege.top() + 6, 3, 15), 1, 1);
            p.drawRoundedRect(QRect(rectSiege.left() + 4, rectSiege.bottom() - 4,
                                    rectSiege.width() - 8, 4), 2, 2);

            p.setFont(idxPax >= 0 ? fGras : fPetite);
            p.setPen(couleurTexte);
            p.drawText(assise.adjusted(0, -1, 0, -1), Qt::AlignCenter, etiquette);
        }

        y += Cabine::PAS_RANGEE_Y;
    }

    y += 4;
    p.setFont(fGras);
    dessinerZoneService(p, xFus, Cabine::LARGEUR_FUSELAGE, y, QStringLiteral("WC"));
}

void PlanAvionWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const QPoint clic = event->pos();
    QString codeClique;
    for (auto it = m_zonesSieges.cbegin(); it != m_zonesSieges.cend(); ++it) {
        if (it.value().contains(clic)) {
            codeClique = it.key();
            break;
        }
    }

    if (codeClique.isEmpty() || m_occupes.contains(codeClique))
        return;

    int posExistante = m_siegesChoisis.indexOf(codeClique);
    if (posExistante >= 0) {
        m_siegesChoisis.removeAt(posExistante);
    } else if (m_siegesChoisis.size() < m_nbPassagers) {
        m_siegesChoisis.append(codeClique);
    } else if (!m_siegesChoisis.isEmpty()) {
        m_siegesChoisis.last() = codeClique;
    }

    update();
    emit selectionModifiee();
}
