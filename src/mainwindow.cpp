#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "billetpdf.h"
#include "calculsvol.h"
#include "constantes.h"
#include "databasemanager.h"
#include "planavionwidget.h"
#include "smtpclient.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSqlQueryModel>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringListModel>
#include <QTableView>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QtMath>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace {

// Graphique léger pour le tableau de bord administrateur
class GraphiqueStatutsWidget : public QWidget
{
public:
    explicit GraphiqueStatutsWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(145);
    }

    void definirValeurs(int attente, int confirmees, int annulees)
    {
        m_attente = attente;
        m_confirmees = confirmees;
        m_annulees = annulees;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int maximum = qMax(1, qMax(m_attente, qMax(m_confirmees, m_annulees)));
        const int margeH = 36;
        const int zoneUtileL = width() - 2 * margeH;
        const int largeurBarre = qMax(45, zoneUtileL / 6);
        const int espacement = (zoneUtileL - 3 * largeurBarre) / 4;
        const int basY = height() - 32;
        const int hauteurMax = qMax(24, basY - 26);

        struct Barre { QString titre; int valeur; QColor couleur; } barres[3] = {
            {QStringLiteral("En attente"), m_attente, QColor(235, 140, 52)},
            {QStringLiteral("Confirmées"), m_confirmees, QColor(21, 128, 61)},
            {QStringLiteral("Annulées"), m_annulees, QColor(225, 97, 98)}
        };

        p.setPen(QPen(QColor(0, 30, 29), 2));
        p.drawLine(margeH / 2, basY, width() - margeH / 2, basY);

        for (int i = 0; i < 3; ++i) {
            int x = margeH + espacement * (i + 1) + largeurBarre * i;
            int h = (barres[i].valeur * hauteurMax) / maximum;
            QRect rectBarre(x, basY - h, largeurBarre, h);

            p.setPen(QPen(QColor(0, 30, 29), 2));
            p.setBrush(barres[i].couleur);
            p.drawRoundedRect(rectBarre, 5, 5);

            p.drawText(QRect(x - 10, basY - h - 22, largeurBarre + 20, 20),
                       Qt::AlignCenter, QString::number(barres[i].valeur));
            p.drawText(QRect(x - 20, basY + 5, largeurBarre + 40, 22),
                       Qt::AlignCenter, barres[i].titre);
        }
    }

private:
    int m_attente = 0;
    int m_confirmees = 0;
    int m_annulees = 0;
};

QIcon creerIconeFlecheBas()
{
    QPixmap px(16, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0, 70, 67));
    p.setPen(Qt::NoPen);
    QPolygon triangle;
    triangle << QPoint(3, 6) << QPoint(13, 6) << QPoint(8, 12);
    p.drawPolygon(triangle);
    return QIcon(px);
}

// Icône de boîte de réception avec pastille rouge très visible en cas de message non lu
QIcon creerIconeBoiteReception(bool avecPastilleNonLu)
{
    QPixmap px(22, 22);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    QRect env(2, 5, 16, 12);
    p.setPen(QPen(QColor(0, 30, 29), 1.8));
    p.setBrush(QColor(255, 255, 254));
    p.drawRoundedRect(env, 2, 2);
    p.drawLine(env.left() + 1, env.top() + 1, env.center().x(), env.center().y() + 1);
    p.drawLine(env.right() - 1, env.top() + 1, env.center().x(), env.center().y() + 1);

    if (avecPastilleNonLu) {
        p.setPen(QPen(QColor(255, 255, 254), 1.6));
        p.setBrush(QColor(225, 97, 98)); // #e16162
        p.drawEllipse(QPoint(17, 5), 5, 5);
    }
    return QIcon(px);
}

// Icône circulaire "Actualiser" (sans aucun texte)
QIcon creerIconeActualiser(const QColor &couleurTrait = QColor(0, 30, 29))
{
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(couleurTrait, 2.2, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRect(3, 3, 14, 14), 45 * 16, 275 * 16);

    p.setPen(Qt::NoPen);
    p.setBrush(couleurTrait);
    QPolygon pointe;
    pointe << QPoint(15, 2) << QPoint(19, 7) << QPoint(13, 7);
    p.drawPolygon(pointe);
    return QIcon(px);
}

QIcon creerIconeAssistance()
{
    QPixmap px(20, 20);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0, 30, 29), 1.6));
    p.setBrush(QColor(249, 188, 96));
    p.drawEllipse(QRect(2, 2, 16, 16));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(9);
    p.setFont(f);
    p.setPen(QColor(0, 30, 29));
    p.drawText(QRect(2, 2, 16, 16), Qt::AlignCenter, QStringLiteral("?"));
    return QIcon(px);
}

// Petits utilitaires pour alléger et standardiser tout le code d'interface
QPushButton *creerBouton(const QString &texte, QWidget *parent, const char *roleBtn = nullptr)
{
    auto *btn = new QPushButton(texte, parent);
    if (roleBtn && *roleBtn)
        btn->setProperty("roleBtn", QString::fromLatin1(roleBtn));
    return btn;
}

QPushButton *creerBoutonActualiserIcone(QWidget *parent)
{
    auto *btn = new QPushButton(parent);
    btn->setIcon(creerIconeActualiser());
    btn->setProperty("roleBtn", QStringLiteral("icone"));
    btn->setToolTip(QStringLiteral("Actualiser"));
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void configurerTableauStandard(QTableView *table)
{
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

QList<AeroportInfo> filtrerAeroportsParTexteVille(const QList<AeroportInfo> &tous,
                                                  const QString &texteSaisi)
{
    QString cle = CalculsVol::normaliserTexteIdentite(texteSaisi);
    if (cle.isEmpty())
        return {};

    QList<AeroportInfo> correspondants;
    for (const AeroportInfo &a : tous) {
        if (CalculsVol::normaliserTexteIdentite(a.affichageVillePays()) == cle
            || CalculsVol::normaliserTexteIdentite(a.ville) == cle
            || CalculsVol::normaliserTexteIdentite(a.codeIata) == cle) {
            correspondants.append(a);
        }
    }
    if (!correspondants.isEmpty())
        return correspondants;

    for (const AeroportInfo &a : tous) {
        QString complet = CalculsVol::normaliserTexteIdentite(a.affichageVillePays());
        if (complet.startsWith(cle) || complet.contains(cle))
            correspondants.append(a);
    }
    return correspondants;
}

bool aeroportCorrespondAuFiltreLibre(const AeroportInfo &a, const QString &texteLibre, int idForce)
{
    if (idForce > 0)
        return a.idAeroport == idForce;
    QString cle = CalculsVol::normaliserTexteIdentite(texteLibre);
    if (cle.isEmpty())
        return true;

    return CalculsVol::normaliserTexteIdentite(a.affichageVillePays()).contains(cle)
           || CalculsVol::normaliserTexteIdentite(a.ville).contains(cle)
           || CalculsVol::normaliserTexteIdentite(a.pays).contains(cle)
           || CalculsVol::normaliserTexteIdentite(a.codeIata).contains(cle);
}

QString traduireStatutReservation(const QString &statutSql)
{
    if (statutSql == QStringLiteral("confirmee"))
        return QStringLiteral("Confirmée");
    if (statutSql == QStringLiteral("annulee"))
        return QStringLiteral("Annulée");
    return QStringLiteral("En attente");
}

// Silhouette vectorielle d'avion de ligne centrée sur (0, 0) orientée vers le haut
QPainterPath construireCheminSilhouetteAvion(double echelle)
{
    QPainterPath ch;
    const double s = echelle;
    // Nez et fuselage avant
    ch.moveTo(0, -28 * s);
    ch.quadTo(4.2 * s, -22 * s, 4.2 * s, -10 * s);
    // Aile droite fléchée
    ch.lineTo(26 * s, 3 * s);
    ch.lineTo(26 * s, 7.5 * s);
    ch.lineTo(4.0 * s, 1.5 * s);
    // Arrière du fuselage + empennage droit
    ch.lineTo(3.2 * s, 17 * s);
    ch.lineTo(11.5 * s, 23 * s);
    ch.lineTo(11.5 * s, 26 * s);
    ch.lineTo(1.8 * s, 23.5 * s);
    ch.lineTo(0, 27 * s);
    // Symétrie côté gauche
    ch.lineTo(-1.8 * s, 23.5 * s);
    ch.lineTo(-11.5 * s, 26 * s);
    ch.lineTo(-11.5 * s, 23 * s);
    ch.lineTo(-3.2 * s, 17 * s);
    ch.lineTo(-4.0 * s, 1.5 * s);
    ch.lineTo(-26 * s, 7.5 * s);
    ch.lineTo(-26 * s, 3 * s);
    ch.lineTo(-4.2 * s, -10 * s);
    ch.quadTo(-4.2 * s, -22 * s, 0, -28 * s);
    ch.closeSubpath();
    return ch;
}

// Filigranes d'avion très discrets sur le fond #abd1c6 (sans jamais gêner écritures ni couleurs)
void dessinerFiligranesAvionFond(QPainter &p, const QRect &zone)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor couleurFiligrane(0, 70, 67, 14);      // ~5,5 % d'opacité : très doux
    const QColor couleurSillage(0, 70, 67, 11);
    const QPainterPath avion = construireCheminSilhouetteAvion(0.82);

    const int pasX = 240;
    const int pasY = 175;
    int ligneIdx = 0;

    for (int y = 55; y < zone.height() + 90; y += pasY, ++ligneIdx) {
        const int decalage = (ligneIdx % 2 == 1) ? (pasX / 2) : 35;
        for (int x = decalage; x < zone.width() + 90; x += pasX) {
            const double angleDeg = ((ligneIdx + (x / pasX)) % 2 == 0) ? 52.0 : -48.0;

            p.save();
            p.translate(x, y);
            p.rotate(angleDeg);

            // Petit sillage pointillé discret derrière l'avion
            QPen penSillage(couleurSillage, 1.4, Qt::DashLine, Qt::RoundCap);
            p.setPen(penSillage);
            p.setBrush(Qt::NoBrush);
            p.drawLine(QPointF(0, 30), QPointF(0, 66));

            p.setPen(Qt::NoPen);
            p.setBrush(couleurFiligrane);
            p.drawPath(avion);
            p.restore();
        }
    }
    p.restore();
}

// Illustration Néo-Brutaliste Gumroad d'un avion (écran d'accueil + panneau déco à droite des vols)
class IllustrationAvionTicketFlyWidget : public QWidget
{
public:
    explicit IllustrationAvionTicketFlyWidget(bool modePanneauLateral, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_modeLateral(modePanneauLateral)
    {
        if (m_modeLateral) {
            setMinimumWidth(185);
            setMaximumWidth(295);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        } else {
            setFixedSize(295, 215);
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QColor encre(0x00, 0x1E, 0x1D);
        const QColor vertProfond(0x00, 0x46, 0x43);
        const QColor menthe(0xAB, 0xD1, 0xC6);
        const QColor jaune(0xF9, 0xBC, 0x60);
        const QColor ivoire(0xFF, 0xFF, 0xFE);
        const QColor grisDoux(0xE8, 0xE4, 0xE6);
        const QColor corail(0xE1, 0x61, 0x62);

        // Ombre dure Néo-Brutaliste façon Gumroad + carte vert profond
        QRect cadre = rect().adjusted(2, 2, -6, -6);
        p.setPen(Qt::NoPen);
        p.setBrush(encre);
        p.drawRoundedRect(cadre.translated(4, 4), 12, 12);

        p.setPen(QPen(encre, 2.2));
        p.setBrush(vertProfond);
        p.drawRoundedRect(cadre, 12, 12);

        p.save();
        QPainterPath clip;
        clip.addRoundedRect(cadre.adjusted(2, 2, -2, -2), 10, 10);
        p.setClipPath(clip);

        const double w = cadre.width();
        const double h = cadre.height();
        const double ox = cadre.left();
        const double oy = cadre.top();

        // Soleil jaune Gumroad
        const double rSoleil = qMin(w, h) * 0.17;
        QPointF centreSoleil(ox + w * 0.77, oy + h * 0.24);
        p.setPen(QPen(encre, 2.0));
        p.setBrush(jaune);
        p.drawEllipse(centreSoleil, rSoleil, rSoleil);

        // Nuages stylisés Néo-Brutalistes
        auto dessinerNuage = [&](double cx, double cy, double ech) {
            QPainterPath nuage;
            nuage.addRoundedRect(QRectF(cx - 34 * ech, cy, 68 * ech, 18 * ech), 9 * ech, 9 * ech);
            nuage.addEllipse(QPointF(cx - 12 * ech, cy - 2 * ech), 14 * ech, 12 * ech);
            nuage.addEllipse(QPointF(cx + 8 * ech, cy - 5 * ech), 17 * ech, 15 * ech);
            p.setPen(QPen(encre, 1.8));
            p.setBrush(grisDoux);
            p.drawPath(nuage.simplified());
        };
        dessinerNuage(ox + w * 0.24, oy + h * 0.24, 0.85);
        dessinerNuage(ox + w * 0.72, oy + h * 0.68, 0.95);

        // Trajectoire aérienne pointillée
        QPainterPath arcVol;
        arcVol.moveTo(ox + w * 0.08, oy + h * 0.80);
        arcVol.cubicTo(ox + w * 0.30, oy + h * 0.65,
                       ox + w * 0.45, oy + h * 0.44,
                       ox + w * 0.86, oy + h * 0.26);
        QPen penArc(menthe, 2.2, Qt::DashLine, Qt::RoundCap);
        p.setPen(penArc);
        p.setBrush(Qt::NoBrush);
        p.drawPath(arcVol);

        // Avion commercial dessiné au centre en montée élégante
        const double echAvion = qBound(0.68, qMin(w / 270.0, h / 220.0), 1.05);
        p.save();
        p.translate(ox + w * 0.47, oy + h * (m_modeLateral ? 0.44 : 0.50));
        p.rotate(-19.0);
        p.scale(echAvion, echAvion);

        // Aile arrière (plan profond)
        QPainterPath aileFond;
        aileFond.moveTo(-8, -4);
        aileFond.lineTo(-34, -34);
        aileFond.lineTo(-18, -36);
        aileFond.lineTo(14, -4);
        aileFond.closeSubpath();
        p.setPen(QPen(encre, 2.0));
        p.setBrush(menthe);
        p.drawPath(aileFond);

        // Dérive verticale jaune Gumroad
        QPainterPath derive;
        derive.moveTo(-64, -4);
        derive.lineTo(-84, -34);
        derive.lineTo(-66, -34);
        derive.lineTo(-44, -4);
        derive.closeSubpath();
        p.setBrush(jaune);
        p.drawPath(derive);

        // Fuselage principal ivoire
        QPainterPath fuselage;
        fuselage.moveTo(-76, -6);
        fuselage.lineTo(54, -8);
        fuselage.quadTo(82, -7, 88, 2);
        fuselage.quadTo(80, 12, 50, 12);
        fuselage.lineTo(-68, 9);
        fuselage.quadTo(-82, 6, -76, -6);
        fuselage.closeSubpath();
        p.setBrush(ivoire);
        p.drawPath(fuselage);

        // Bande décorative corail sur le fuselage
        p.setPen(QPen(corail, 2.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(-58, 3), QPointF(58, 3));

        // Verrière du cockpit + hublots
        p.setPen(QPen(encre, 1.6));
        p.setBrush(menthe);
        QPainterPath cockpit;
        cockpit.moveTo(58, -5);
        cockpit.lineTo(74, -2);
        cockpit.lineTo(70, 2);
        cockpit.lineTo(56, 1);
        cockpit.closeSubpath();
        p.drawPath(cockpit);

        for (int hx = -36; hx <= 36; hx += 12)
            p.drawEllipse(QPointF(hx, -2), 2.4, 2.8);

        // Aile avant au premier plan + réacteur
        QPainterPath aileAvant;
        aileAvant.moveTo(12, 5);
        aileAvant.lineTo(-26, 40);
        aileAvant.lineTo(-8, 42);
        aileAvant.lineTo(30, 6);
        aileAvant.closeSubpath();
        p.setPen(QPen(encre, 2.0));
        p.setBrush(jaune);
        p.drawPath(aileAvant);

        p.setBrush(grisDoux);
        p.drawRoundedRect(QRectF(-2, 18, 22, 9), 4, 4);
        p.restore();

        // En mode panneau latéral (écran Vols), ajout d'une boussole & étiquette déco en bas
        if (m_modeLateral && h > 250) {
            // Petite rose des vents décorative en haut à gauche
            QPointF boussole(ox + 30, oy + 30);
            p.setPen(QPen(menthe, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(boussole, 13, 13);
            p.drawLine(QPointF(boussole.x() - 17, boussole.y()), QPointF(boussole.x() + 17, boussole.y()));
            p.drawLine(QPointF(boussole.x(), boussole.y() - 17), QPointF(boussole.x(), boussole.y() + 17));

            // Étiquette billet style Gumroad en bas du panneau
            QRectF bandeauBas(ox + 12, oy + h - 84, w - 24, 70);
            p.setPen(QPen(encre, 2.0));
            p.setBrush(ivoire);
            p.drawRoundedRect(bandeauBas, 8, 8);

            QFont fTitre = p.font();
            fTitre.setBold(true);
            fTitre.setPointSize(10);
            p.setFont(fTitre);
            p.setPen(encre);
            p.drawText(bandeauBas.adjusted(8, 7, -8, -34),
                       Qt::AlignCenter, QStringLiteral("✈  TICKETFLY"));

            QFont fSous = p.font();
            fSous.setBold(false);
            fSous.setPointSize(8);
            p.setFont(fSous);
            p.setPen(vertProfond);
            p.drawText(bandeauBas.adjusted(8, 28, -8, -6),
                       Qt::AlignCenter | Qt::TextWordWrap,
                       QStringLiteral("Vols directs & escales\nSièges cabine & Billet PDF"));
        }

        p.restore();
    }

private:
    bool m_modeLateral = false;
};

// Panneau décoratif gauche (carnet de route aérien Gumroad avec avion & escales)
class DecoItineraireTicketFlyWidget : public QWidget
{
public:
    explicit DecoItineraireTicketFlyWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumWidth(165);
        setMaximumWidth(235);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QColor encre(0x00, 0x1E, 0x1D);
        const QColor vertProfond(0x00, 0x46, 0x43);
        const QColor menthe(0xAB, 0xD1, 0xC6);
        const QColor jaune(0xF9, 0xBC, 0x60);
        const QColor ivoire(0xFF, 0xFF, 0xFE);

        QRect cadre = rect().adjusted(2, 2, -6, -6);
        p.setPen(Qt::NoPen);
        p.setBrush(encre);
        p.drawRoundedRect(cadre.translated(4, 4), 12, 12);

        p.setPen(QPen(encre, 2.2));
        p.setBrush(ivoire);
        p.drawRoundedRect(cadre, 12, 12);

        const double ox = cadre.left();
        const double oy = cadre.top();
        const double w = cadre.width();
        const double h = cadre.height();

        // Bandeau haut jaune Gumroad
        QRectF entete(ox + 10, oy + 10, w - 20, 30);
        p.setPen(QPen(encre, 1.8));
        p.setBrush(jaune);
        p.drawRoundedRect(entete, 7, 7);

        QFont fTitre = p.font();
        fTitre.setBold(true);
        fTitre.setPointSize(9);
        p.setFont(fTitre);
        p.setPen(encre);
        p.drawText(entete, Qt::AlignCenter, QStringLiteral("LIGNES AÉRIENNES"));

        // Ligne de vol pointillée verticale avec escales illustrées
        const double axeX = ox + w * 0.50;
        const double yHaut = oy + 58;
        const double yBas = qMax(yHaut + 60.0, oy + h - 28.0);

        p.setPen(QPen(vertProfond, 2.0, Qt::DashLine, Qt::RoundCap));
        p.drawLine(QPointF(axeX, yHaut), QPointF(axeX, yBas));

        const QStringList codes = {
            QStringLiteral("TNR"), QStringLiteral("NOS"),
            QStringLiteral("RUN"), QStringLiteral("CDG")
        };
        for (int i = 0; i < codes.size(); ++i) {
            double yPt = yHaut + (yBas - yHaut) * (static_cast<double>(i) / (codes.size() - 1));
            QRectF badge(axeX - 26, yPt - 11, 52, 22);
            p.setPen(QPen(encre, 1.7));
            p.setBrush(i == 0 || i == codes.size() - 1 ? menthe : ivoire);
            p.drawRoundedRect(badge, 6, 6);
            p.setPen(encre);
            p.drawText(badge, Qt::AlignCenter, codes[i]);
        }

        // Silhouette d'avion remontant la route aérienne
        if (yBas - yHaut > 120) {
            p.save();
            p.translate(axeX + 34, (yHaut + yBas) * 0.48);
            p.rotate(-18.0);
            p.setPen(QPen(encre, 1.6));
            p.setBrush(jaune);
            p.drawPath(construireCheminSilhouetteAvion(0.62));
            p.restore();
        }
    }
};

// Petits drapeaux vectoriels nets pour chaque devise (compatibles Windows / Linux sans police emoji)
QIcon creerIconeDrapeauDevise(const QString &codeMonnaie)
{
    QPixmap px(24, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, false);

    QRect r(1, 1, 21, 13);
    if (codeMonnaie == QStringLiteral("MGA")) {
        // Drapeau de Madagascar : bande blanche à gauche, rouge en haut, vert en bas
        p.fillRect(QRect(1, 1, 7, 13), QColor(255, 255, 255));
        p.fillRect(QRect(8, 1, 14, 7), QColor(252, 61, 50));
        p.fillRect(QRect(8, 8, 14, 6), QColor(0, 126, 58));
    } else if (codeMonnaie == QStringLiteral("EUR")) {
        // Drapeau Européen : fond bleu roi + couronne d'étoiles dorées
        p.fillRect(r, QColor(0, 51, 153));
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 204, 0));
        for (int i = 0; i < 8; ++i) {
            double ang = (i * 2.0 * M_PI) / 8.0;
            p.drawEllipse(QPointF(11.5 + 4.2 * qCos(ang), 7.5 + 4.2 * qSin(ang)), 1.1, 1.1);
        }
    } else if (codeMonnaie == QStringLiteral("USD")) {
        // Drapeau États-Unis : bandes rouges/blanches + canton bleu
        p.fillRect(r, QColor(255, 255, 255));
        for (int y = 1; y < 14; y += 4)
            p.fillRect(QRect(1, y, 21, 2), QColor(178, 34, 52));
        p.fillRect(QRect(1, 1, 10, 7), QColor(60, 59, 110));
    } else if (codeMonnaie == QStringLiteral("GBP")) {
        // Drapeau Royaume-Uni simplifié : fond bleu marine + croix blanche et rouge
        p.fillRect(r, QColor(1, 33, 105));
        p.fillRect(QRect(9, 1, 5, 13), QColor(255, 255, 255));
        p.fillRect(QRect(1, 5, 21, 5), QColor(255, 255, 255));
        p.fillRect(QRect(10, 1, 3, 13), QColor(200, 16, 46));
        p.fillRect(QRect(1, 6, 21, 3), QColor(200, 16, 46));
    } else {
        // Drapeau Maurice (MUR) : rouge, bleu, jaune, vert
        p.fillRect(QRect(1, 1, 21, 3), QColor(234, 40, 57));
        p.fillRect(QRect(1, 4, 21, 3), QColor(26, 32, 109));
        p.fillRect(QRect(1, 7, 21, 4), QColor(255, 213, 0));
        p.fillRect(QRect(1, 11, 21, 3), QColor(0, 165, 81));
    }

    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QColor(0, 30, 29));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);
    return QIcon(px);
}

QString lireCoordPaiementTicketFly(const QString &cle, const QString &valeurDefaut)
{
    QSettings cfg(DatabaseManager::cheminFichierConfig(), QSettings::IniFormat);
    QString lu = cfg.value(QStringLiteral("paiement/") + cle).toString().trimmed();
    return lu.isEmpty() ? valeurDefaut : lu;
}

} // namespace

MainWindow::MainWindow(DatabaseManager *db, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_db(db)
{
    ui->setupUi(this);

    initialiserBarreOutils();
    construirePageAccueil();
    construirePageAcces();
    construirePageReservation();
    construirePageMesReservations();
    construirePageAdmin();
    construirePageBoiteReception();

    rechargerDonneesReference();
    initialiserRechercheEtAutocompletion();
    mettreAJourEtatSessionUI();

    // Application de la devise initiale (ou mémorisée dans config.ini)
    changerMonnaieGlobale();

    // Préchargement des vols et ouverture sur l'écran d'accueil minimal TicketFly
    executerRechercheOuAffichageComplet(false);
    ui->pilePages->setCurrentIndex(PageAccueil);

    // Défilement automatique fluide vers le haut pour le tableau des vols
    m_minuteurDefilementVols = new QTimer(this);
    connect(m_minuteurDefilementVols, &QTimer::timeout,
            this, &MainWindow::faireDefilerTableauVols);
    m_minuteurDefilementVols->start(Constantes::INTERVALLE_DEFILEMENT_VOLS_MS);

    // Sondage périodique (4 s) pour synchroniser pastille & tableaux entre instances
    m_minuteurSondage = new QTimer(this);
    connect(m_minuteurSondage, &QTimer::timeout, this, &MainWindow::sonderBaseDeDonnees);
    m_minuteurSondage->start(Constantes::INTERVALLE_SONDAGE_MS);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent *)
{
    // Fond vert d'eau Gumroad (#abd1c6) + filigranes d'avion ultra-discrets qui ne tachent aucun texte
    QPainter p(this);
    p.fillRect(rect(), QColor(0xAB, 0xD1, 0xC6));
    dessinerFiligranesAvionFond(p, rect());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_decoGaucheVols)
        m_decoGaucheVols->setVisible(width() >= 1220);
}

QString MainWindow::prixFormate(double montantEur) const
{
    return Constantes::formaterMontant(montantEur, m_codeMonnaie);
}

void MainWindow::initialiserBarreOutils()
{
    auto *barre = new QToolBar(QStringLiteral("Navigation"), this);
    barre->setMovable(false);
    barre->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    addToolBar(Qt::TopToolBarArea, barre);

    m_actAccueil = barre->addAction(QStringLiteral("Accueil"), this, [this]() {
        ui->pilePages->setCurrentIndex(PageAccueil);
    });

    m_actRecherche = barre->addAction(QStringLiteral("Vols"), this, [this]() {
        ui->pilePages->setCurrentIndex(PageRecherche);
        executerRechercheOuAffichageComplet(false);
    });

    // Boîte de réception placée bien en haut à gauche avec indicateur visuel de non-lu
    m_actBoiteReception = barre->addAction(
        creerIconeBoiteReception(false), QStringLiteral("Boîte de réception"), this, [this]() {
            if (m_idUtilisateur <= 0) {
                QMessageBox::information(this,
                                         QStringLiteral("Connexion requise"),
                                         QStringLiteral("Connectez-vous pour consulter vos notifications."));
                ui->pilePages->setCurrentIndex(PageAcces);
                return;
            }
            actualiserBoiteReception();
            ui->pilePages->setCurrentIndex(PageBoiteReception);
        });

    m_actMesReservations = barre->addAction(QStringLiteral("Mes réservations"), this, [this]() {
        if (m_idUtilisateur <= 0) {
            QMessageBox::information(this,
                                     QStringLiteral("Connexion requise"),
                                     QStringLiteral("Connectez-vous pour consulter vos réservations."));
            ui->pilePages->setCurrentIndex(PageAcces);
            return;
        }
        actualiserMesReservations();
        ui->pilePages->setCurrentIndex(PageMesReservations);
    });

    m_actAdmin = barre->addAction(QStringLiteral("Administration"), this, [this]() {
        actualiserEspaceAdmin();
        ui->pilePages->setCurrentIndex(PageAdmin);
    });

    auto *espaceur = new QWidget(this);
    espaceur->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    barre->addWidget(espaceur);

    // Sélecteur global de monnaie avec petits drapeaux et recalcul automatique partout
    m_comboMonnaie = new QComboBox(barre);
    m_comboMonnaie->setIconSize(QSize(22, 15));
    m_comboMonnaie->setToolTip(QStringLiteral("Choisir la devise d'affichage (recalcul automatique)"));
    m_comboMonnaie->addItem(creerIconeDrapeauDevise(QStringLiteral("MGA")),
                            QStringLiteral("MGA (Ar)"), QStringLiteral("MGA"));
    m_comboMonnaie->addItem(creerIconeDrapeauDevise(QStringLiteral("EUR")),
                            QStringLiteral("EUR (€)"), QStringLiteral("EUR"));
    m_comboMonnaie->addItem(creerIconeDrapeauDevise(QStringLiteral("USD")),
                            QStringLiteral("USD ($)"), QStringLiteral("USD"));
    m_comboMonnaie->addItem(creerIconeDrapeauDevise(QStringLiteral("GBP")),
                            QStringLiteral("GBP (£)"), QStringLiteral("GBP"));
    m_comboMonnaie->addItem(creerIconeDrapeauDevise(QStringLiteral("MUR")),
                            QStringLiteral("MUR (Rs)"), QStringLiteral("MUR"));

    QSettings cfgDevise(DatabaseManager::cheminFichierConfig(), QSettings::IniFormat);
    QString deviseSauvee = cfgDevise.value(QStringLiteral("interface/monnaie"),
                                           QStringLiteral("MGA")).toString().toUpper();
    int idxDevise = m_comboMonnaie->findData(deviseSauvee);
    m_comboMonnaie->setCurrentIndex(idxDevise >= 0 ? idxDevise : 0);
    connect(m_comboMonnaie, &QComboBox::currentIndexChanged,
            this, &MainWindow::changerMonnaieGlobale);
    barre->addWidget(m_comboMonnaie);

    // Bouton Actualiser global SANS écriture (icône seule) pour Client et Admin
    auto *btnActualiserHaut = new QToolButton(barre);
    btnActualiserHaut->setIcon(creerIconeActualiser(QColor(255, 255, 254)));
    btnActualiserHaut->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btnActualiserHaut->setToolTip(QStringLiteral("Actualiser"));
    btnActualiserHaut->setCursor(Qt::PointingHandCursor);
    connect(btnActualiserHaut, &QToolButton::clicked, this, &MainWindow::actualiserVueActive);
    barre->addWidget(btnActualiserHaut);

    QAction *actDeuxiemeInstance = barre->addAction(
        QStringLiteral("Nouvelle fenêtre"), this, [this]() {
            if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList())) {
                QMessageBox::warning(this,
                                     QStringLiteral("Ouverture impossible"),
                                     QStringLiteral("Impossible de lancer une seconde fenêtre."));
            }
        });
    actDeuxiemeInstance->setToolTip(QStringLiteral("Ouvrir une 2e instance (Client + Admin)"));

    m_actAssistance = barre->addAction(
        creerIconeAssistance(), QStringLiteral("Assistance"), this, [this]() {
            ouvrirDialogueAssistance();
        });

    m_lblUtilisateurConnecte = new QLabel(this);
    m_lblUtilisateurConnecte->setContentsMargins(6, 0, 6, 0);
    barre->addWidget(m_lblUtilisateurConnecte);

    m_actSession = barre->addAction(QStringLiteral("Connexion"), this, [this]() {
        if (m_idUtilisateur > 0)
            seDeconnecter();
        else
            ui->pilePages->setCurrentIndex(PageAcces);
    });
}

void MainWindow::construirePageAccueil()
{
    auto *pageAccueil = new QWidget(ui->pilePages);
    auto *layoutExterne = new QVBoxLayout(pageAccueil);
    layoutExterne->setContentsMargins(36, 24, 36, 24);
    layoutExterne->addStretch(1);

    auto *ligneCentree = new QHBoxLayout();
    ligneCentree->addStretch(1);

    auto *carte = new QFrame(pageAccueil);
    carte->setObjectName(QStringLiteral("carteAccueilGumroad"));
    carte->setMaximumWidth(830);

    auto *layoutCarte = new QHBoxLayout(carte);
    layoutCarte->setContentsMargins(32, 28, 32, 28);
    layoutCarte->setSpacing(28);

    // Colonne gauche : message d'accueil minimal « Bienvenue dans TicketFly » + accès direct
    auto *colGauche = new QVBoxLayout();
    colGauche->setSpacing(12);

    auto *ligneBadge = new QHBoxLayout();
    auto *badge = new QLabel(QStringLiteral("✈  AGENCE TICKETFLY"), carte);
    badge->setObjectName(QStringLiteral("badgeTicketFly"));
    ligneBadge->addWidget(badge);
    ligneBadge->addStretch();
    colGauche->addLayout(ligneBadge);

    auto *lblTitre = new QLabel(QStringLiteral("Bienvenue dans TicketFly"), carte);
    lblTitre->setStyleSheet(QStringLiteral("font-size: 26px; font-weight: 800; color: #001e1d;"));
    lblTitre->setWordWrap(true);
    colGauche->addWidget(lblTitre);

    auto *lblSousTitre = new QLabel(
        QStringLiteral("Consultez les vols des prochaines semaines, composez vos itinéraires directs "
                       "ou avec escale, choisissez vos sièges sur le plan d'avion et recevez votre billet PDF."),
        carte);
    lblSousTitre->setStyleSheet(QStringLiteral("font-size: 13px; color: #004643; font-weight: 600;"));
    lblSousTitre->setWordWrap(true);
    colGauche->addWidget(lblSousTitre);

    colGauche->addSpacing(6);

    auto *ligneActions = new QHBoxLayout();
    ligneActions->setSpacing(10);
    auto *btnVoirVols = creerBouton(QStringLiteral("Explorer et réserver un vol"), carte);
    m_btnSecondaireAccueil = creerBouton(QStringLiteral("Connexion / Inscription"), carte, "secondaire");
    ligneActions->addWidget(btnVoirVols);
    ligneActions->addWidget(m_btnSecondaireAccueil);
    ligneActions->addStretch();
    colGauche->addLayout(ligneActions);

    connect(btnVoirVols, &QPushButton::clicked, this, [this]() {
        ui->pilePages->setCurrentIndex(PageRecherche);
        executerRechercheOuAffichageComplet(false);
    });
    connect(m_btnSecondaireAccueil, &QPushButton::clicked, this, [this]() {
        if (m_idUtilisateur <= 0) {
            ui->pilePages->setCurrentIndex(PageAcces);
        } else if (m_role == QStringLiteral("admin") || m_role == QStringLiteral("super_admin")) {
            actualiserEspaceAdmin();
            ui->pilePages->setCurrentIndex(PageAdmin);
        } else {
            actualiserMesReservations();
            ui->pilePages->setCurrentIndex(PageMesReservations);
        }
    });

    // Colonne droite : illustration dessinée de l'avion dans le style Gumroad
    auto *illustrationAvion = new IllustrationAvionTicketFlyWidget(false, carte);

    layoutCarte->addLayout(colGauche, 3);
    layoutCarte->addWidget(illustrationAvion, 0, Qt::AlignCenter);

    ligneCentree->addWidget(carte, 1);
    ligneCentree->addStretch(1);
    layoutExterne->addLayout(ligneCentree);
    layoutExterne->addStretch(1);

    ui->pilePages->addWidget(pageAccueil); // PageAccueil = 6
}

void MainWindow::changerMonnaieGlobale()
{
    if (!m_comboMonnaie)
        return;

    const QString nouvelleMonnaie = m_comboMonnaie->currentData().toString();
    if (nouvelleMonnaie.isEmpty())
        return;

    const double nouveauTaux = Constantes::tauxMonnaieDepuisEur(nouvelleMonnaie);
    const QString sym = Constantes::symboleMonnaie(nouvelleMonnaie);
    const int decimales = (nouvelleMonnaie == QStringLiteral("MGA")) ? 0 : 2;

    // Conversion automatique du plafond « Prix max » et du tarif Admin sans déclencher de filtre parasite
    {
        QSignalBlocker bloc(ui->spinPrixMax);
        double valeurEur = ui->spinPrixMax->value() / qMax(0.0001, m_tauxMonnaiePrecedent);
        ui->spinPrixMax->setDecimals(decimales);
        ui->spinPrixMax->setMinimum(qRound(35.0 * nouveauTaux));
        ui->spinPrixMax->setMaximum(20000.0 * nouveauTaux);
        ui->spinPrixMax->setSingleStep(nouvelleMonnaie == QStringLiteral("MGA") ? 50000.0 : 50.0);
        ui->spinPrixMax->setSuffix(QLatin1Char(' ') + sym);
        ui->spinPrixMax->setValue(valeurEur * nouveauTaux);
    }

    if (m_spinPrixAdmin) {
        QSignalBlocker blocAdmin(m_spinPrixAdmin);
        double valAdminEur = m_spinPrixAdmin->value() / qMax(0.0001, m_tauxMonnaiePrecedent);
        m_spinPrixAdmin->setDecimals(decimales);
        m_spinPrixAdmin->setMaximum(25000.0 * nouveauTaux);
        m_spinPrixAdmin->setSingleStep(nouvelleMonnaie == QStringLiteral("MGA") ? 50000.0 : 25.0);
        m_spinPrixAdmin->setSuffix(QStringLiteral(" %1 (0 = auto)").arg(sym));
        m_spinPrixAdmin->setValue(valAdminEur * nouveauTaux);
    }

    m_codeMonnaie = nouvelleMonnaie;
    m_tauxMonnaiePrecedent = nouveauTaux;

    QSettings cfg(DatabaseManager::cheminFichierConfig(), QSettings::IniFormat);
    cfg.setValue(QStringLiteral("interface/monnaie"), m_codeMonnaie);

    if (m_modeleRecherche) {
        m_modeleRecherche->setHeaderData(
            7, Qt::Horizontal, QStringLiteral("Prix (%1)").arg(sym));
    }

    if (m_planAvion)
        m_planAvion->definirMonnaie(m_codeMonnaie);

    if (m_lblLegendePlan) {
        m_lblLegendePlan->setText(
            QStringLiteral("Jaune : Classe A (+%1)  •  Blanc : Régulier (0 %2)  •  Vert : Choisi  •  Gris : Occupé")
                .arg(prixFormate(Constantes::SUPPLEMENT_SIEGE_CLASSE_A_EUR), sym));
    }

    mettreAJourApercuReference();
    executerRechercheOuAffichageComplet(false);
    mettreAJourSiegesEtTarifReservation();
    actualiserInstructionsPaiement();

    if (m_idUtilisateur > 0 && ui->pilePages->currentIndex() == PageMesReservations)
        actualiserMesReservations();
    if ((m_role == QStringLiteral("admin") || m_role == QStringLiteral("super_admin"))
        && ui->pilePages->currentIndex() == PageAdmin) {
        actualiserEspaceAdmin();
    }
}

void MainWindow::actualiserVueActive()
{
    rechargerDonneesReference();
    mettreAJourPastilleBoiteReception();

    switch (ui->pilePages->currentIndex()) {
    case PageRecherche:
        executerRechercheOuAffichageComplet(false);
        break;
    case PageMesReservations:
        actualiserMesReservations();
        break;
    case PageBoiteReception:
        actualiserBoiteReception();
        break;
    case PageAdmin:
        actualiserEspaceAdmin();
        break;
    default:
        break;
    }
    statusBar()->showMessage(QStringLiteral("Données actualisées."), 2500);
}

void MainWindow::rechargerDonneesReference()
{
    if (!m_db || !m_db->estConnecte())
        return;

    m_aeroports = m_db->chargerAeroports();
    m_liaisons = m_db->chargerLiaisons();
    m_villesFormatees.clear();
    for (const AeroportInfo &a : m_aeroports) {
        QString libelle = a.affichageVillePays();
        if (!m_villesFormatees.contains(libelle))
            m_villesFormatees.append(libelle);
    }
    m_villesFormatees.sort(Qt::CaseInsensitive);
}

void MainWindow::installerAutocompletionVille(QLineEdit *champ)
{
    auto *modele = new QStringListModel(m_villesFormatees, champ);
    auto *completeur = new QCompleter(modele, champ);
    completeur->setCaseSensitivity(Qt::CaseInsensitive);
    completeur->setFilterMode(Qt::MatchContains);
    completeur->setCompletionMode(QCompleter::PopupCompletion);
    completeur->setMaxVisibleItems(10);

    // Correction explicite du fond clair sur le menu déroulant des villes/pays (évite tout conflit sombre)
    if (QAbstractItemView *popup = completeur->popup()) {
        popup->setObjectName(QStringLiteral("popupVilles"));
        popup->setStyleSheet(QStringLiteral(
            "QAbstractItemView {"
            "  background-color: #fffffe;"
            "  color: #001e1d;"
            "  border: 2px solid #001e1d;"
            "  selection-background-color: #f9bc60;"
            "  selection-color: #001e1d;"
            "  outline: none;"
            "}"
            "QAbstractItemView::item { padding: 6px 10px; background-color: #fffffe; color: #001e1d; }"
            "QAbstractItemView::item:hover { background-color: #abd1c6; color: #001e1d; }"
            "QAbstractItemView::item:selected { background-color: #f9bc60; color: #001e1d; font-weight: bold; }"));
    }

    champ->setCompleter(completeur);

    QAction *actionFleche = champ->addAction(creerIconeFlecheBas(), QLineEdit::TrailingPosition);
    actionFleche->setToolTip(QStringLiteral("Afficher toutes les destinations"));
    connect(actionFleche, &QAction::triggered, this, [champ, completeur]() {
        champ->setFocus();
        completeur->setCompletionPrefix(QString());
        completeur->complete();
    });
}

void MainWindow::initialiserRechercheEtAutocompletion()
{
    installerAutocompletionVille(ui->editDepart);
    installerAutocompletionVille(ui->editArrivee);
    installerAutocompletionVille(ui->editEscale);

    ui->dateDepart->setMinimumDate(QDate::currentDate());
    ui->dateDepart->setDate(QDate::currentDate().addDays(1));
    ui->dateDepart->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    ui->chkDate->setChecked(false);
    ui->dateDepart->setEnabled(false);
    ui->spinPrixMax->setEnabled(false);

    ui->lblCalculTrajet->setObjectName(QStringLiteral("carteInfo"));
    ui->lblCalculTrajet->hide();

    // Bouton icône Actualiser sans écriture juste à côté des actions de recherche
    auto *btnActuVols = creerBoutonActualiserIcone(ui->pageRecherche);
    ui->ligneFiltres->insertWidget(ui->ligneFiltres->indexOf(ui->btnReserverSelection), btnActuVols);
    connect(btnActuVols, &QPushButton::clicked, this, [this]() {
        executerRechercheOuAffichageComplet(false);
    });

    connect(ui->chkDate, &QCheckBox::toggled, this, [this](bool actif) {
        ui->dateDepart->setEnabled(actif);
        if (!m_blocageSignauxFiltres)
            executerRechercheOuAffichageComplet(false);
    });
    connect(ui->dateDepart, &QDateEdit::dateChanged, this, [this]() {
        if (!m_blocageSignauxFiltres && ui->chkDate->isChecked())
            executerRechercheOuAffichageComplet(false);
    });
    connect(ui->chkPrixMax, &QCheckBox::toggled, this, [this](bool actif) {
        ui->spinPrixMax->setEnabled(actif);
        if (!m_blocageSignauxFiltres)
            executerRechercheOuAffichageComplet(false);
    });
    connect(ui->spinPrixMax, &QDoubleSpinBox::valueChanged, this, [this]() {
        if (!m_blocageSignauxFiltres && ui->chkPrixMax->isChecked())
            executerRechercheOuAffichageComplet(false);
    });

    connect(ui->editDepart, &QLineEdit::textChanged, this, &MainWindow::actualiserAeroportsDepart);
    connect(ui->editArrivee, &QLineEdit::textChanged, this, &MainWindow::actualiserAeroportsArrivee);
    connect(ui->editEscale, &QLineEdit::textChanged, this, [this]() {
        if (!m_blocageSignauxFiltres)
            executerRechercheOuAffichageComplet(false);
    });
    connect(ui->comboAeroportDepart, &QComboBox::currentIndexChanged, this, [this]() {
        mettreAJourApercuReference();
        if (!m_blocageSignauxFiltres)
            executerRechercheOuAffichageComplet(false);
    });
    connect(ui->comboAeroportArrivee, &QComboBox::currentIndexChanged, this, [this]() {
        mettreAJourApercuReference();
        if (!m_blocageSignauxFiltres)
            executerRechercheOuAffichageComplet(false);
    });
    connect(ui->spinPassagers, &QSpinBox::valueChanged, this, [this]() {
        mettreAJourApercuReference();
        if (!m_blocageSignauxFiltres)
            executerRechercheOuAffichageComplet(false);
    });

    connect(ui->btnRechercher, &QPushButton::clicked, this, &MainWindow::lancerRecherche);
    connect(ui->btnReinitialiserFiltres, &QPushButton::clicked,
            this, &MainWindow::reinitialiserFiltres);
    connect(ui->btnReserverSelection, &QPushButton::clicked,
            this, &MainWindow::ouvrirFormulaireReservation);

    m_modeleRecherche = new QStandardItemModel(0, 9, this);
    m_modeleRecherche->setHorizontalHeaderLabels({
        QStringLiteral("Vol"),
        QStringLiteral("Trajet"),
        QStringLiteral("Escales"),
        QStringLiteral("Départ"),
        QStringLiteral("Arrivée"),
        QStringLiteral("Durée"),
        QStringLiteral("Distance"),
        QStringLiteral("Prix (€)"),
        QStringLiteral("Places")
    });

    ui->tableVols->setModel(m_modeleRecherche);
    ui->tableVols->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableVols->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableVols->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableVols->setAlternatingRowColors(true);
    ui->tableVols->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableVols->verticalHeader()->hide();
    ui->tableVols->verticalHeader()->setDefaultSectionSize(38);

    // Colonnes ajustées au contenu réel (sans grand vide dans « Escales ») + panneau déco avion à droite
    QHeaderView *entete = ui->tableVols->horizontalHeader();
    entete->setMinimumSectionSize(56);
    entete->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Vol
    entete->setSectionResizeMode(1, QHeaderView::Stretch);          // Trajet
    entete->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Escales (ajusté pile au texte !)
    entete->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Départ
    entete->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Arrivée
    entete->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Durée
    entete->setSectionResizeMode(6, QHeaderView::ResizeToContents); // Distance
    entete->setSectionResizeMode(7, QHeaderView::ResizeToContents); // Prix
    entete->setSectionResizeMode(8, QHeaderView::ResizeToContents); // Places

    // On encadre le tableau des vols (ajusté au texte) par des illustrations d'avion à gauche et à droite
    ui->layoutRecherche->removeWidget(ui->tableVols);
    auto *zoneTableauEtDeco = new QHBoxLayout();
    zoneTableauEtDeco->setSpacing(10);
    m_decoGaucheVols = new DecoItineraireTicketFlyWidget(ui->pageRecherche);
    m_decoGaucheVols->setVisible(width() >= 1220);
    zoneTableauEtDeco->addWidget(m_decoGaucheVols, 1);
    zoneTableauEtDeco->addWidget(ui->tableVols, 5);
    zoneTableauEtDeco->addWidget(new IllustrationAvionTicketFlyWidget(true, ui->pageRecherche), 1);
    ui->layoutRecherche->addLayout(zoneTableauEtDeco, 1);

    connect(ui->tableVols, &QTableView::doubleClicked,
            this, &MainWindow::ouvrirFormulaireReservation);

    m_blocageSignauxFiltres = true;
    actualiserAeroportsDepart();
    actualiserAeroportsArrivee();
    m_blocageSignauxFiltres = false;
}

void MainWindow::actualiserComboAeroportVille(const QLineEdit *champVille, QComboBox *combo)
{
    combo->blockSignals(true);
    combo->clear();

    QList<AeroportInfo> liste = filtrerAeroportsParTexteVille(m_aeroports, champVille->text());
    QString premiereVille = liste.isEmpty() ? QString() : liste.first().ville;
    QList<AeroportInfo> memeVille;
    for (const AeroportInfo &a : liste) {
        if (a.ville.compare(premiereVille, Qt::CaseInsensitive) == 0)
            memeVille.append(a);
    }

    // On n'affiche le sous-menu d'aéroport que si la ville choisie possède au moins 2 aéroports (ex: Paris)
    if (memeVille.size() > 1) {
        combo->addItem(QStringLiteral("Tous (%1)").arg(memeVille.size()), 0);
        for (const AeroportInfo &a : memeVille)
            combo->addItem(a.affichageAeroportCourt(), a.idAeroport);
        combo->setEnabled(true);
        combo->show();
    } else {
        if (memeVille.size() == 1)
            combo->addItem(memeVille.first().affichageAeroportCourt(), memeVille.first().idAeroport);
        combo->setEnabled(false);
        combo->hide();
    }

    combo->blockSignals(false);
    mettreAJourApercuReference();
    if (!m_blocageSignauxFiltres)
        executerRechercheOuAffichageComplet(false);
}

void MainWindow::actualiserAeroportsDepart()
{
    actualiserComboAeroportVille(ui->editDepart, ui->comboAeroportDepart);
}

void MainWindow::actualiserAeroportsArrivee()
{
    actualiserComboAeroportVille(ui->editArrivee, ui->comboAeroportArrivee);
}

QList<AeroportInfo> MainWindow::resoudreAeroportsSelectionnes(const QLineEdit *champVille,
                                                              const QComboBox *comboAeroport) const
{
    int idChoisi = comboAeroport->currentData().toInt();
    if (idChoisi > 0) {
        for (const AeroportInfo &a : m_aeroports) {
            if (a.idAeroport == idChoisi)
                return {a};
        }
    }

    QList<AeroportInfo> correspondants = filtrerAeroportsParTexteVille(m_aeroports, champVille->text());
    if (correspondants.isEmpty())
        return {};

    QString villeCible = correspondants.first().ville;
    QList<AeroportInfo> resultat;
    for (const AeroportInfo &a : correspondants) {
        if (a.ville.compare(villeCible, Qt::CaseInsensitive) == 0)
            resultat.append(a);
    }
    return resultat;
}

void MainWindow::mettreAJourApercuReference()
{
    QList<AeroportInfo> deps = resoudreAeroportsSelectionnes(ui->editDepart, ui->comboAeroportDepart);
    QList<AeroportInfo> arrs = resoudreAeroportsSelectionnes(ui->editArrivee, ui->comboAeroportArrivee);

    if (deps.isEmpty() || arrs.isEmpty() || deps.first().idAeroport == arrs.first().idAeroport) {
        ui->lblCalculTrajet->hide();
        return;
    }

    const AeroportInfo &dep = deps.first();
    const AeroportInfo &arr = arrs.first();
    LiaisonReference ref = CalculsVol::calculerReferenceTroncon(dep, arr, m_liaisons);
    int nbPax = ui->spinPassagers->value();

    ui->lblCalculTrajet->setText(
        QStringLiteral("%1 (%2) → %3 (%4)   •   %5 km   •   Durée directe : %6   •   Base : %7 / pers. (%8 pour %9)")
            .arg(dep.ville, dep.codeIata, arr.ville, arr.codeIata,
                 QString::number(ref.distanceKm),
                 CalculsVol::formaterDureeMinutes(ref.dureeMinutes),
                 prixFormate(ref.prixBase),
                 prixFormate(ref.prixBase * nbPax),
                 QString::number(nbPax)));
    ui->lblCalculTrajet->show();
}

void MainWindow::reinitialiserFiltres()
{
    m_blocageSignauxFiltres = true;
    ui->editDepart->clear();
    ui->editArrivee->clear();
    ui->editEscale->clear();
    ui->chkDate->setChecked(false);
    ui->dateDepart->setDate(QDate::currentDate().addDays(1));
    ui->dateDepart->setEnabled(false);
    ui->chkPrixMax->setChecked(false);
    ui->spinPrixMax->setEnabled(false);
    ui->spinPassagers->setValue(1);
    actualiserComboAeroportVille(ui->editDepart, ui->comboAeroportDepart);
    actualiserComboAeroportVille(ui->editArrivee, ui->comboAeroportArrivee);
    m_blocageSignauxFiltres = false;

    executerRechercheOuAffichageComplet(false);
}

void MainWindow::lancerRecherche()
{
    executerRechercheOuAffichageComplet(true);
}

void MainWindow::executerRechercheOuAffichageComplet(bool signalerErreursSaisie)
{
    if (!m_db || !m_db->estConnecte())
        return;

    if (ui->chkDate->isChecked() && ui->dateDepart->date() < QDate::currentDate()) {
        ui->dateDepart->setDate(QDate::currentDate());
        if (signalerErreursSaisie) {
            QMessageBox::warning(this, QStringLiteral("Date invalide"),
                                 QStringLiteral("Impossible de rechercher un vol dans le passé."));
            return;
        }
    }

    const QString saisieDep = ui->editDepart->text().trimmed();
    const QString saisieArr = ui->editArrivee->text().trimmed();
    const QList<TronconVol> volsProgrammes = m_db->chargerVolsProgrammes();
    const QDateTime maintenant = QDateTime::currentDateTime();
    const QDateTime dateLimiteSemaines = maintenant.addDays(Constantes::HORIZON_PROCHAINS_VOLS_JOURS);

    const double plafondEur = ui->spinPrixMax->value()
                              / Constantes::tauxMonnaieDepuisEur(m_codeMonnaie);

    // Cas 1 : Départ ET Arrivée saisis -> recherche complète avec escales automatiques (0 à 2 max)
    if (!saisieDep.isEmpty() && !saisieArr.isEmpty()) {
        QList<AeroportInfo> deps = resoudreAeroportsSelectionnes(ui->editDepart, ui->comboAeroportDepart);
        QList<AeroportInfo> arrs = resoudreAeroportsSelectionnes(ui->editArrivee, ui->comboAeroportArrivee);

        if (deps.isEmpty() || arrs.isEmpty()) {
            if (signalerErreursSaisie) {
                QMessageBox::warning(this, QStringLiteral("Villes non reconnues"),
                                     QStringLiteral("Choisissez une ville de départ et d'arrivée valides."));
            }
            m_itinerairesTrouves.clear();
            remplirTableauItineraires();
            return;
        }

        if (deps.size() == 1 && arrs.size() == 1
            && deps.first().idAeroport == arrs.first().idAeroport) {
            if (signalerErreursSaisie) {
                QMessageBox::warning(this, QStringLiteral("Trajet invalide"),
                                     QStringLiteral("Le départ et l'arrivée doivent être différents."));
            }
            m_itinerairesTrouves.clear();
            remplirTableauItineraires();
            return;
        }

        QString texteEscale = ui->editEscale->text().trimmed();
        if (!texteEscale.isEmpty()) {
            QList<AeroportInfo> esc = filtrerAeroportsParTexteVille(m_aeroports, texteEscale);
            if (esc.isEmpty()) {
                if (signalerErreursSaisie) {
                    QMessageBox::warning(this, QStringLiteral("Escale inconnue"),
                                         QStringLiteral("Cette ville d'escale n'existe pas dans la base."));
                }
                m_itinerairesTrouves.clear();
                remplirTableauItineraires();
                return;
            }
            texteEscale = esc.first().ville;
        }

        QList<ItinerairePropose> bruts = CalculsVol::rechercherItineraires(
            deps, arrs, m_aeroports, m_liaisons, volsProgrammes,
            ui->chkDate->isChecked(), ui->dateDepart->date(),
            ui->spinPassagers->value(),
            ui->chkPrixMax->isChecked(), plafondEur,
            texteEscale);

        m_itinerairesTrouves.clear();
        for (const ItinerairePropose &it : bruts) {
            if (!ui->chkDate->isChecked() && it.dateDepartInitiale() > dateLimiteSemaines)
                continue;
            m_itinerairesTrouves.append(it);
        }
        remplirTableauItineraires();
        return;
    }

    // Cas 2 : Affichage automatique des vols à venir dans les prochaines semaines (filtrés souplement)
    m_itinerairesTrouves.clear();
    const int idDepChoisi = ui->comboAeroportDepart->isVisible()
                                ? ui->comboAeroportDepart->currentData().toInt()
                                : 0;
    const int idArrChoisi = ui->comboAeroportArrivee->isVisible()
                                ? ui->comboAeroportArrivee->currentData().toInt()
                                : 0;

    for (const TronconVol &vol : volsProgrammes) {
        if (vol.dateDepart < maintenant)
            continue;
        if (!ui->chkDate->isChecked() && vol.dateDepart > dateLimiteSemaines)
            continue;
        if (vol.placesDisponibles < ui->spinPassagers->value())
            continue;
        if (!aeroportCorrespondAuFiltreLibre(vol.depart, saisieDep, idDepChoisi))
            continue;
        if (!aeroportCorrespondAuFiltreLibre(vol.arrivee, saisieArr, idArrChoisi))
            continue;
        if (ui->chkDate->isChecked() && vol.dateDepart.date() != ui->dateDepart->date())
            continue;
        if (ui->chkPrixMax->isChecked() && vol.prixUnitaire > plafondEur)
            continue;

        ItinerairePropose it;
        it.troncons.append(vol);
        it.distanceTotaleKm = vol.distanceKm;
        it.dureeTotaleMinutes = vol.dureeMinutes;
        it.prixParPassager = vol.prixUnitaire;
        it.placesMinDisponibles = vol.placesDisponibles;
        m_itinerairesTrouves.append(it);
    }

    remplirTableauItineraires();
}

QList<QStandardItem *> MainWindow::construireLigneVol(const ItinerairePropose &it) const
{
    const TronconVol &premier = it.troncons.first();
    const TronconVol &dernier = it.troncons.last();
    const QString prixStr = prixFormate(it.prixParPassager);
    const QString infoBulle = QStringLiteral("%1\n%2 — %3 / passager")
                                  .arg(it.resumeTrajet(),
                                       it.descriptionEscales(),
                                       prixStr);

    // Dans la colonne Prix, on affiche le montant formaté sans répéter le symbole déjà présent en en-tête
    QString montantCellule = prixStr.section(QLatin1Char(' '), 0, -2);
    if (montantCellule.isEmpty())
        montantCellule = prixStr;

    QList<QStandardItem *> ligne;
    ligne << new QStandardItem(it.numerosVolsAffiches())
          << new QStandardItem(it.trajetCourt())
          << new QStandardItem(it.descriptionEscales())
          << new QStandardItem(premier.dateDepart.toString(QStringLiteral("dd/MM HH:mm")))
          << new QStandardItem(dernier.dateArrivee.toString(QStringLiteral("dd/MM HH:mm")))
          << new QStandardItem(CalculsVol::formaterDureeMinutes(it.dureeTotaleMinutes))
          << new QStandardItem(QStringLiteral("%1 km").arg(it.distanceTotaleKm))
          << new QStandardItem(montantCellule)
          << new QStandardItem(QString::number(it.placesMinDisponibles));

    for (QStandardItem *cellule : ligne)
        cellule->setToolTip(infoBulle);

    return ligne;
}

void MainWindow::remplirTableauItineraires()
{
    m_modeleRecherche->removeRows(0, m_modeleRecherche->rowCount());

    for (const ItinerairePropose &it : m_itinerairesTrouves)
        m_modeleRecherche->appendRow(construireLigneVol(it));

    if (!m_itinerairesTrouves.isEmpty()) {
        ui->tableVols->selectRow(0);
        ui->tableVols->verticalScrollBar()->setValue(0);
    }
}

void MainWindow::faireDefilerTableauVols()
{
    if (ui->pilePages->currentIndex() != PageRecherche || m_itinerairesTrouves.size() <= 1)
        return;

    // On met le défilement en pause dès que la souris survole le tableau pour faciliter le clic
    if (ui->tableVols->underMouse()
        || ui->editDepart->hasFocus()
        || ui->editArrivee->hasFocus()
        || ui->editEscale->hasFocus()) {
        return;
    }

    QScrollBar *barre = ui->tableVols->verticalScrollBar();
    if (barre && barre->maximum() > 0) {
        int suivant = barre->value() + 1;
        barre->setValue(suivant > barre->maximum() ? 0 : suivant);
        return;
    }

    // Si la fenêtre est assez grande pour tout afficher sans barre de défilement,
    // on fait remonter doucement la première ligne vers le bas toutes les ~2.4 s
    ++m_compteurTicksDefilement;
    if (m_compteurTicksDefilement < Constantes::TICKS_ROTATION_VOLS_PLEIN_ECRAN)
        return;
    m_compteurTicksDefilement = 0;

    int ligneChoisie = ui->tableVols->currentIndex().row();
    ItinerairePropose premier = m_itinerairesTrouves.takeFirst();
    m_itinerairesTrouves.append(premier);

    QList<QStandardItem *> itemsPremier = m_modeleRecherche->takeRow(0);
    m_modeleRecherche->appendRow(itemsPremier);

    if (ligneChoisie == 0)
        ui->tableVols->selectRow(m_itinerairesTrouves.size() - 1);
    else if (ligneChoisie > 0)
        ui->tableVols->selectRow(ligneChoisie - 1);
}

// ============================================================================
// PAGE 1 : CONNEXION & INSCRIPTION SIMPLIFIÉES
// ============================================================================

void MainWindow::construirePageAcces()
{
    auto *layoutPage = new QVBoxLayout(ui->pageAcces);
    layoutPage->setContentsMargins(120, 30, 120, 30);

    m_ongletsAcces = new QTabWidget(ui->pageAcces);
    layoutPage->addWidget(m_ongletsAcces, 1);

    auto *validateurNom = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[\\p{L} '\\-]{2,80}$")), this);
    auto *validateurEmailSaisie = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[^\\s+]{0,140}$")), this);

    // Onglet Connexion
    auto *widgetConnexion = new QWidget(m_ongletsAcces);
    auto *formLogin = new QFormLayout(widgetConnexion);
    formLogin->setContentsMargins(35, 30, 35, 30);
    formLogin->setSpacing(12);

    m_loginEmail = new QLineEdit(widgetConnexion);
    m_loginEmail->setPlaceholderText(QStringLiteral("votre.email@domaine.com"));
    m_loginEmail->setValidator(validateurEmailSaisie);

    m_loginMdp = new QLineEdit(widgetConnexion);
    m_loginMdp->setEchoMode(QLineEdit::Password);
    m_loginMdp->setPlaceholderText(QStringLiteral("Mot de passe"));

    auto *btnLogin = creerBouton(QStringLiteral("Se connecter"), widgetConnexion, "confirmer");
    auto *btnOubliMdp = creerBouton(QStringLiteral("Mot de passe oublié ?"), widgetConnexion, "avertissement");
    auto *btnVisiteur = creerBouton(QStringLiteral("Retour aux vols"), widgetConnexion, "secondaire");

    auto *ligneBoutonsLogin = new QHBoxLayout();
    ligneBoutonsLogin->addWidget(btnLogin);
    ligneBoutonsLogin->addWidget(btnOubliMdp);
    ligneBoutonsLogin->addWidget(btnVisiteur);

    formLogin->addRow(QStringLiteral("Email :"), m_loginEmail);
    formLogin->addRow(QStringLiteral("Mot de passe :"), m_loginMdp);
    formLogin->addRow(QString(), ligneBoutonsLogin);

    connect(btnLogin, &QPushButton::clicked, this, &MainWindow::traiterConnexion);
    connect(m_loginMdp, &QLineEdit::returnPressed, this, &MainWindow::traiterConnexion);
    connect(btnOubliMdp, &QPushButton::clicked, this, [this]() {
        if (!m_loginEmail->text().trimmed().isEmpty())
            m_emailUtilisateur = m_loginEmail->text().trimmed();
        ouvrirDialogueAssistance(QStringLiteral("Mot de passe oublié / Compte bloqué"));
    });
    connect(btnVisiteur, &QPushButton::clicked, this, &MainWindow::passerEnModeVisiteur);

    m_ongletsAcces->addTab(widgetConnexion, QStringLiteral("Connexion"));

    // Onglet Inscription
    auto *widgetInscr = new QWidget(m_ongletsAcces);
    auto *formInscr = new QFormLayout(widgetInscr);
    formInscr->setContentsMargins(35, 25, 35, 25);
    formInscr->setSpacing(10);

    m_inscrNom = new QLineEdit(widgetInscr);
    m_inscrNom->setValidator(validateurNom);
    m_inscrPrenom = new QLineEdit(widgetInscr);
    m_inscrPrenom->setValidator(validateurNom);
    m_inscrEmail = new QLineEdit(widgetInscr);
    m_inscrEmail->setValidator(validateurEmailSaisie);
    m_inscrEmail->setPlaceholderText(QStringLiteral("prenom.nom@domaine.com (sans alias +)"));

    m_inscrMdp = new QLineEdit(widgetInscr);
    m_inscrMdp->setEchoMode(QLineEdit::Password);
    m_inscrMdpConfirm = new QLineEdit(widgetInscr);
    m_inscrMdpConfirm->setEchoMode(QLineEdit::Password);

    auto *btnInscr = creerBouton(QStringLiteral("Créer mon compte"), widgetInscr, "confirmer");
    connect(btnInscr, &QPushButton::clicked, this, &MainWindow::traiterInscription);

    formInscr->addRow(QStringLiteral("Nom :"), m_inscrNom);
    formInscr->addRow(QStringLiteral("Prénom :"), m_inscrPrenom);
    formInscr->addRow(QStringLiteral("Email :"), m_inscrEmail);
    formInscr->addRow(QStringLiteral("Mot de passe :"), m_inscrMdp);
    formInscr->addRow(QStringLiteral("Confirmer :"), m_inscrMdpConfirm);
    formInscr->addRow(QString(), btnInscr);

    m_ongletsAcces->addTab(widgetInscr, QStringLiteral("Inscription"));
}

void MainWindow::traiterConnexion()
{
    int id = 0;
    QString role, nomComplet, emailNorm, erreur;

    if (!m_db->connecterUtilisateur(m_loginEmail->text(), m_loginMdp->text(),
                                    id, role, nomComplet, emailNorm, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Connexion refusée"), erreur);
        return;
    }

    m_idUtilisateur = id;
    m_role = role;
    m_nomUtilisateur = nomComplet;
    m_emailUtilisateur = emailNorm;
    m_loginMdp->clear();

    m_dernierIdAttenteConnu = m_db->obtenirStatistiquesAdmin().dernierIdEnAttente;
    m_derniereEmpreinteClient = m_db->empreinteReservationsClient(m_idUtilisateur);
    m_dernierNbNonLusConnu = m_db->compterMessagesNonLus(m_idUtilisateur, m_role);

    mettreAJourEtatSessionUI();

    if (m_role == QStringLiteral("admin") || m_role == QStringLiteral("super_admin")) {
        actualiserEspaceAdmin();
        ui->pilePages->setCurrentIndex(PageAdmin);
    } else {
        ui->pilePages->setCurrentIndex(PageRecherche);
    }
}

void MainWindow::traiterInscription()
{
    const QString nom = m_inscrNom->text().trimmed();
    const QString prenom = m_inscrPrenom->text().trimmed();
    const QString email = m_inscrEmail->text().trimmed().toLower();
    const QString mdp = m_inscrMdp->text();

    // 1. Vérifications de base AVANT d'envoyer le code par mail
    if (nom.size() < 2 || prenom.size() < 2) {
        QMessageBox::warning(this, QStringLiteral("Champs incomplets"),
                             QStringLiteral("Le nom et le prénom doivent contenir au moins 2 lettres."));
        return;
    }

    QString erreurEmail;
    if (!CalculsVol::validerEmailStrict(email, erreurEmail)) {
        QMessageBox::warning(this, QStringLiteral("Email invalide"), erreurEmail);
        return;
    }

    if (mdp.size() < Constantes::LONGUEUR_MDP_MIN) {
        QMessageBox::warning(this, QStringLiteral("Mot de passe trop court"),
                             QStringLiteral("Le mot de passe doit contenir au moins %1 caractères.")
                                 .arg(Constantes::LONGUEUR_MDP_MIN));
        return;
    }

    if (mdp != m_inscrMdpConfirm->text()) {
        QMessageBox::warning(this, QStringLiteral("Confirmation incorrecte"),
                             QStringLiteral("Les deux mots de passe ne correspondent pas."));
        return;
    }

    // 2. Vérifier que le serveur SMTP est bien configuré dans config.ini
    ConfigSmtp cfgSmtp = SmtpClient::chargerConfiguration();
    if (!cfgSmtp.estConfigure()) {
        QMessageBox::warning(
            this, QStringLiteral("SMTP non configuré"),
            QStringLiteral("Configurez d'abord l'email SMTP dans Administration → Configuration (config.ini) "
                           "pour pouvoir envoyer le code de vérification."));
        return;
    }

    // 3. Générer et envoyer le code à 6 chiffres par email
    QString codeAttendu;
    auto envoyerNouveauCode = [&]() -> bool {
        int tirage = QRandomGenerator::global()->bounded(100000, 1000000);
        codeAttendu = QString::number(tirage);

        QString errSmtp;
        QString sujet = QStringLiteral("TicketFly — Votre code de confirmation : ") + codeAttendu;
        QString corps = QStringLiteral(
                            "Bonjour %1 %2,\n\n"
                            "Voici votre code à 6 chiffres pour valider votre inscription sur TicketFly :\n\n"
                            "    %3\n\n"
                            "Saisissez ce code dans la fenêtre de confirmation de l'application.\n"
                            "À très vite sur TicketFly !")
                            .arg(prenom, nom.toUpper(), codeAttendu);

        if (!SmtpClient::envoyerEmailTexteSimple(cfgSmtp, email, sujet, corps, errSmtp)) {
            QMessageBox::critical(this, QStringLiteral("Échec d'envoi du code"),
                                  QStringLiteral("Impossible d'envoyer l'email à %1 :\n%2").arg(email, errSmtp));
            return false;
        }
        return true;
    };

    if (!envoyerNouveauCode())
        return;

    // 4. Fenêtre avant connexion (stylée automatiquement par style.qss façon Gumroad)
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Confirmation de l'adresse email"));
    dlg.setModal(true);
    dlg.setMinimumWidth(430);

    auto *layoutExterne = new QVBoxLayout(&dlg);
    layoutExterne->setContentsMargins(18, 18, 18, 18);

    auto *carte = new QFrame(&dlg);
    carte->setObjectName(QStringLiteral("carteAccueilGumroad"));
    auto *layoutCarte = new QVBoxLayout(carte);
    layoutCarte->setContentsMargins(22, 20, 22, 20);
    layoutCarte->setSpacing(12);

    auto *badge = new QLabel(QStringLiteral("✈  VÉRIFICATION EMAIL"), carte);
    badge->setObjectName(QStringLiteral("badgeTicketFly"));
    badge->setMaximumWidth(210);
    layoutCarte->addWidget(badge);

    auto *lblInfo = new QLabel(
        QStringLiteral("Un code à <b>6 chiffres</b> a été envoyé à :<br><b>%1</b><br>"
                       "Saisissez-le ci-dessous pour activer votre compte :").arg(email.toHtmlEscaped()),
        carte);
    lblInfo->setWordWrap(true);
    layoutCarte->addWidget(lblInfo);

    auto *editCode = new QLineEdit(carte);
    editCode->setPlaceholderText(QStringLiteral("Code à 6 chiffres (ex : 482915)"));
    editCode->setMaxLength(6);
    editCode->setAlignment(Qt::AlignCenter);
    editCode->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: bold; letter-spacing: 6px; padding: 8px;"));
    editCode->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[0-9]{0,6}$")), editCode));
    layoutCarte->addWidget(editCode);

    auto *ligneBoutons = new QHBoxLayout();
    auto *btnAnnuler = creerBouton(QStringLiteral("Annuler"), carte, "secondaire");
    auto *btnRenvoyer = creerBouton(QStringLiteral("Renvoyer le code"), carte, "avertissement");
    auto *btnValider = creerBouton(QStringLiteral("Valider mon compte"), carte, "confirmer");
    ligneBoutons->addWidget(btnAnnuler);
    ligneBoutons->addWidget(btnRenvoyer);
    ligneBoutons->addWidget(btnValider);
    layoutCarte->addLayout(ligneBoutons);

    layoutExterne->addWidget(carte);

    connect(btnAnnuler, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnRenvoyer, &QPushButton::clicked, &dlg, [&]() {
        if (envoyerNouveauCode()) {
            editCode->clear();
            QMessageBox::information(&dlg, QStringLiteral("Code renvoyé"),
                                     QStringLiteral("Un nouveau code vient d'être envoyé à %1.").arg(email));
        }
    });
    connect(btnValider, &QPushButton::clicked, &dlg, [&]() {
        if (editCode->text().trimmed() != codeAttendu) {
            QMessageBox::warning(&dlg, QStringLiteral("Code incorrect"),
                                 QStringLiteral("Le code saisi ne correspond pas. Vérifiez votre boîte mail."));
            return;
        }
        dlg.accept();
    });
    connect(editCode, &QLineEdit::returnPressed, btnValider, &QPushButton::click);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // 5. Le code est validé -> création réelle du compte en base et connexion automatique
    QString erreur;
    if (!m_db->inscrireClient(nom, prenom, email, mdp, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Inscription refusée"), erreur);
        return;
    }

    int nouvelId = 0;
    QString role, nomComplet, emailNorm;
    if (!m_db->connecterUtilisateur(email, mdp, nouvelId, role, nomComplet, emailNorm, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Connexion refusée"), erreur);
        return;
    }

    QString ignoreErr;
    m_db->envoyerMessageBoite(
        nouvelId, QStringLiteral("client"), 0,
        QStringLiteral("TicketFly"),
        QStringLiteral("noreply@billets-avion.mg"),
        QStringLiteral("Compte"),
        QStringLiteral("Bienvenue dans TicketFly ! Email vérifié"),
        QStringLiteral("Votre adresse email a bien été confirmée. Vous recevrez ici vos billets PDF et notifications."),
        ignoreErr);

    m_db->envoyerMessageBoite(
        0, QStringLiteral("admin"), nouvelId,
        nomComplet, emailNorm,
        QStringLiteral("Inscription"),
        QStringLiteral("Nouveau client vérifié : %1").arg(nomComplet),
        QStringLiteral("Nouveau compte client inscrit (email vérifié par code) : %1 (%2).").arg(nomComplet, emailNorm),
        ignoreErr);

    m_idUtilisateur = nouvelId;
    m_role = role;
    m_nomUtilisateur = nomComplet;
    m_emailUtilisateur = emailNorm;
    m_derniereEmpreinteClient = m_db->empreinteReservationsClient(m_idUtilisateur);
    m_dernierNbNonLusConnu = m_db->compterMessagesNonLus(m_idUtilisateur, m_role);

    m_inscrNom->clear();
    m_inscrPrenom->clear();
    m_inscrEmail->clear();
    m_inscrMdp->clear();
    m_inscrMdpConfirm->clear();

    mettreAJourEtatSessionUI();
    ui->pilePages->setCurrentIndex(PageRecherche);
}

void MainWindow::passerEnModeVisiteur()
{
    ui->pilePages->setCurrentIndex(PageRecherche);
}

void MainWindow::seDeconnecter()
{
    m_idUtilisateur = 0;
    m_role = QStringLiteral("visiteur");
    m_nomUtilisateur = QStringLiteral("Visiteur");
    m_emailUtilisateur.clear();
    m_dernierIdAttenteConnu = -1;
    m_dernierNbNonLusConnu = 0;
    m_derniereEmpreinteClient.clear();
    mettreAJourEtatSessionUI();
    ui->pilePages->setCurrentIndex(PageRecherche);
    executerRechercheOuAffichageComplet(false);
}

void MainWindow::mettreAJourEtatSessionUI()
{
    const bool estConnecte = (m_idUtilisateur > 0);
    const bool estAdmin = (m_role == QStringLiteral("admin")
                           || m_role == QStringLiteral("super_admin"));
    const bool estSuperAdmin = (m_role == QStringLiteral("super_admin"));

    m_actAdmin->setVisible(estAdmin);
    m_actBoiteReception->setVisible(estConnecte);
    m_actMesReservations->setVisible(estConnecte);

    if (m_ongletsAdmin && m_indexOngletSuperAdmin >= 0)
        m_ongletsAdmin->setTabVisible(m_indexOngletSuperAdmin, estSuperAdmin);

    if (estConnecte) {
        m_lblUtilisateurConnecte->setText(m_nomUtilisateur);
        m_actSession->setText(QStringLiteral("Déconnexion"));
        setWindowTitle(QStringLiteral("TicketFly — %1").arg(m_nomUtilisateur));
        if (m_btnSecondaireAccueil)
            m_btnSecondaireAccueil->setText(estAdmin ? QStringLiteral("Tableau de bord Admin")
                                                     : QStringLiteral("Mes réservations"));
    } else {
        m_lblUtilisateurConnecte->setText(QStringLiteral("Visiteur"));
        m_actSession->setText(QStringLiteral("Connexion"));
        setWindowTitle(QStringLiteral("TicketFly — Réservation de billets d'avion"));
        if (m_btnSecondaireAccueil)
            m_btnSecondaireAccueil->setText(QStringLiteral("Connexion / Inscription"));
    }

    mettreAJourPastilleBoiteReception();
}

void MainWindow::mettreAJourPastilleBoiteReception()
{
    if (!m_actBoiteReception)
        return;

    int nonLus = (m_idUtilisateur > 0 && m_db && m_db->estConnecte())
                     ? m_db->compterMessagesNonLus(m_idUtilisateur, m_role)
                     : 0;
    m_actBoiteReception->setIcon(creerIconeBoiteReception(nonLus > 0));
    m_actBoiteReception->setText(
        nonLus > 0 ? QStringLiteral("Boîte de réception (%1)").arg(nonLus)
                   : QStringLiteral("Boîte de réception"));
}

// ============================================================================
// PAGE 2 : RÉSERVATION CLIENT ÉPURÉE + PLAN D'AVION MODERNE
// ============================================================================

void MainWindow::construirePageReservation()
{
    auto *layoutPrincipal = new QVBoxLayout(ui->pageReservation);

    m_lblResumeReservation = new QLabel(ui->pageReservation);
    m_lblResumeReservation->setObjectName(QStringLiteral("carteInfo"));
    m_lblResumeReservation->setWordWrap(true);
    layoutPrincipal->addWidget(m_lblResumeReservation);

    auto *corpsMilieu = new QHBoxLayout();

    // Colonne gauche : fiches passagers épurées
    auto *scrollPax = new QScrollArea(ui->pageReservation);
    scrollPax->setWidgetResizable(true);
    m_conteneurPassagers = new QWidget(scrollPax);
    m_layoutPassagers = new QVBoxLayout(m_conteneurPassagers);
    scrollPax->setWidget(m_conteneurPassagers);
    corpsMilieu->addWidget(scrollPax, 3);

    // Colonne droite : plan d'avion interactif
    auto *groupePlan = new QGroupBox(QStringLiteral("Choix des sièges à bord"), ui->pageReservation);
    auto *layoutPlan = new QVBoxLayout(groupePlan);

    m_lblLegendePlan = new QLabel(
        QStringLiteral("Jaune : Classe A (+%1)  •  Blanc : Régulier  •  Vert : Choisi  •  Gris : Occupé")
            .arg(prixFormate(Constantes::SUPPLEMENT_SIEGE_CLASSE_A_EUR)),
        groupePlan);
    m_lblLegendePlan->setWordWrap(true);
    layoutPlan->addWidget(m_lblLegendePlan);

    auto *scrollPlan = new QScrollArea(groupePlan);
    scrollPlan->setWidgetResizable(true);
    scrollPlan->setMinimumWidth(455);
    m_planAvion = new PlanAvionWidget(scrollPlan);
    scrollPlan->setWidget(m_planAvion);
    layoutPlan->addWidget(scrollPlan, 1);

    m_lblTarifSieges = new QLabel(groupePlan);
    m_lblTarifSieges->setObjectName(QStringLiteral("carteInfo"));
    m_lblTarifSieges->setWordWrap(true);
    layoutPlan->addWidget(m_lblTarifSieges);

    connect(m_planAvion, &PlanAvionWidget::selectionModifiee,
            this, &MainWindow::mettreAJourSiegesEtTarifReservation);

    corpsMilieu->addWidget(groupePlan, 2);
    layoutPrincipal->addLayout(corpsMilieu, 1);

    // Bloc compact de paiement TicketFly (Mobile Money ou Carte bancaire)
    auto *groupePaiement = new QGroupBox(QStringLiteral("Paiement TicketFly"), ui->pageReservation);
    auto *layoutPaiement = new QVBoxLayout(groupePaiement);
    layoutPaiement->setSpacing(6);

    m_lblInstructionsPaiement = new QLabel(groupePaiement);
    m_lblInstructionsPaiement->setObjectName(QStringLiteral("carteInfo"));
    m_lblInstructionsPaiement->setWordWrap(true);
    layoutPaiement->addWidget(m_lblInstructionsPaiement);

    auto *ligneChampsPaiement = new QHBoxLayout();
    ligneChampsPaiement->setSpacing(8);

    m_comboModePaiement = new QComboBox(groupePaiement);
    m_comboModePaiement->addItem(QStringLiteral("Mobile Money"));
    m_comboModePaiement->addItem(QStringLiteral("Carte bancaire"));

    m_editContactPaiement = new QLineEdit(groupePaiement);
    m_editReferencePaiement = new QLineEdit(groupePaiement);
    m_editReferencePaiement->setPlaceholderText(QStringLiteral("Référence du transfert / reçu (obligatoire)"));

    ligneChampsPaiement->addWidget(new QLabel(QStringLiteral("Mode :"), groupePaiement));
    ligneChampsPaiement->addWidget(m_comboModePaiement);
    ligneChampsPaiement->addWidget(m_editContactPaiement, 2);
    ligneChampsPaiement->addWidget(m_editReferencePaiement, 2);
    layoutPaiement->addLayout(ligneChampsPaiement);

    connect(m_comboModePaiement, &QComboBox::currentIndexChanged,
            this, &MainWindow::actualiserInstructionsPaiement);
    layoutPrincipal->addWidget(groupePaiement);

    auto *barreBas = new QHBoxLayout();
    auto *btnRetour = creerBouton(QStringLiteral("Retour"), ui->pageReservation, "secondaire");
    auto *btnValider = creerBouton(QStringLiteral("Confirmer la demande de réservation"),
                                   ui->pageReservation, "confirmer");

    connect(btnRetour, &QPushButton::clicked, this, [this]() {
        ui->pilePages->setCurrentIndex(PageRecherche);
    });
    connect(btnValider, &QPushButton::clicked, this, &MainWindow::validerDemandeReservation);

    barreBas->addWidget(btnRetour);
    barreBas->addStretch();
    barreBas->addWidget(btnValider);
    layoutPrincipal->addLayout(barreBas);
}

void MainWindow::ouvrirFormulaireReservation()
{
    QModelIndex idx = ui->tableVols->currentIndex();
    if (!idx.isValid() || idx.row() < 0 || idx.row() >= m_itinerairesTrouves.size()) {
        QMessageBox::information(this, QStringLiteral("Sélection requise"),
                                 QStringLiteral("Sélectionnez un vol dans le tableau."));
        return;
    }

    if (m_idUtilisateur <= 0) {
        QMessageBox::information(this, QStringLiteral("Connexion requise"),
                                 QStringLiteral("Connectez-vous avant de réserver un vol."));
        ui->pilePages->setCurrentIndex(PageAcces);
        return;
    }

    m_itineraireEnCours = m_itinerairesTrouves.at(idx.row());
    int nbPassagers = ui->spinPassagers->value();

    if (m_itineraireEnCours.troncons.first().dateDepart < QDateTime::currentDateTime()) {
        QMessageBox::warning(this, QStringLiteral("Départ passé"),
                             QStringLiteral("Ce vol est déjà parti."));
        return;
    }

    QLayoutItem *enfant = nullptr;
    while ((enfant = m_layoutPassagers->takeAt(0)) != nullptr) {
        if (enfant->widget())
            enfant->widget()->deleteLater();
        delete enfant;
    }
    m_blocsPassagers.clear();

    auto *validateurIdentite = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[\\p{L} '\\-]{2,80}$")), m_conteneurPassagers);
    auto *validateurPasseport = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9]{6,12}$")), m_conteneurPassagers);

    for (int i = 0; i < nbPassagers; ++i) {
        auto *groupe = new QGroupBox(QStringLiteral("Passager %1 (P%1)").arg(i + 1), m_conteneurPassagers);
        auto *form = new QFormLayout(groupe);

        BlocPassagerUI bloc;
        bloc.lblSiegeAttribue = new QLabel(QStringLiteral("Cliquez un siège à droite"), groupe);

        bloc.editNom = new QLineEdit(groupe);
        bloc.editNom->setValidator(validateurIdentite);
        bloc.editNom->setPlaceholderText(QStringLiteral("Nom exact"));

        bloc.editPrenom = new QLineEdit(groupe);
        bloc.editPrenom->setValidator(validateurIdentite);
        bloc.editPrenom->setPlaceholderText(QStringLiteral("Prénom(s)"));

        bloc.dateNaissance = new QDateEdit(QDate(2000, 1, 1), groupe);
        bloc.dateNaissance->setCalendarPopup(true);
        bloc.dateNaissance->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
        bloc.dateNaissance->setMinimumDate(QDate::currentDate().addYears(-Constantes::AGE_MAX_PASSAGER_ANS));
        bloc.dateNaissance->setMaximumDate(QDate::currentDate());

        bloc.editNumeroPasseport = new QLineEdit(groupe);
        bloc.editNumeroPasseport->setValidator(validateurPasseport);
        bloc.editNumeroPasseport->setPlaceholderText(QStringLiteral("Ex : MG4829103"));

        auto *ligneImport = new QHBoxLayout();
        auto *btnImport = creerBouton(QStringLiteral("Importer PDF/PNG"), groupe, "secondaire");
        bloc.lblStatutVerification = new QLabel(QStringLiteral("Non importé"), groupe);
        bloc.lblStatutVerification->setWordWrap(true);
        ligneImport->addWidget(btnImport);
        ligneImport->addWidget(bloc.lblStatutVerification, 1);

        form->addRow(QStringLiteral("Siège :"), bloc.lblSiegeAttribue);
        form->addRow(QStringLiteral("Nom :"), bloc.editNom);
        form->addRow(QStringLiteral("Prénom :"), bloc.editPrenom);
        form->addRow(QStringLiteral("Naissance :"), bloc.dateNaissance);
        form->addRow(QStringLiteral("N° Passeport :"), bloc.editNumeroPasseport);
        form->addRow(QStringLiteral("Fichier :"), ligneImport);

        m_blocsPassagers.append(bloc);
        m_layoutPassagers->addWidget(groupe);

        connect(btnImport, &QPushButton::clicked, this, [this, i]() { importerPasseportPassager(i); });
        connect(bloc.editNom, &QLineEdit::textChanged, this, [this, i]() { verifierConcordancePassager(i); });
        connect(bloc.editPrenom, &QLineEdit::textChanged, this, [this, i]() { verifierConcordancePassager(i); });
        connect(bloc.editNumeroPasseport, &QLineEdit::textChanged, this, [this, i]() { verifierConcordancePassager(i); });
    }
    m_layoutPassagers->addStretch();

    QSet<QString> occupes;
    if (!m_itineraireEnCours.troncons.isEmpty() && m_itineraireEnCours.troncons.first().idVolExistant > 0)
        occupes = m_db->chargerSiegesOccupesVol(m_itineraireEnCours.troncons.first().idVolExistant);
    m_planAvion->definirMonnaie(m_codeMonnaie);
    m_planAvion->configurerCabine(nbPassagers, occupes);
    actualiserInstructionsPaiement();

    ui->pilePages->setCurrentIndex(PageReservation);
}

void MainWindow::mettreAJourSiegesEtTarifReservation()
{
    if (!m_planAvion || m_itineraireEnCours.troncons.isEmpty())
        return;

    const QStringList choisis = m_planAvion->siegesSelectionnes();
    const int nbPax = m_blocsPassagers.size();

    for (int i = 0; i < nbPax; ++i) {
        if (i < choisis.size()) {
            m_blocsPassagers[i].lblSiegeAttribue->setStyleSheet(QStringLiteral("color: #15803d; font-weight: bold;"));
            m_blocsPassagers[i].lblSiegeAttribue->setText(
                PlanAvionWidget::formaterLibelleSiege(choisis[i], m_codeMonnaie));
        } else {
            m_blocsPassagers[i].lblSiegeAttribue->setStyleSheet(QStringLiteral("color: #b45309; font-weight: bold;"));
            m_blocsPassagers[i].lblSiegeAttribue->setText(QStringLiteral("À choisir sur l'avion"));
        }
    }

    const double baseVols = m_itineraireEnCours.prixParPassager * nbPax;
    const double supplement = m_planAvion->calculerSupplementTotal();
    const double totalGeneral = baseVols + supplement;

    m_lblResumeReservation->setText(
        QStringLiteral("<b>%1</b>  •  %2  •  Départ : %3  •  <b>Total : %4</b>")
            .arg(m_itineraireEnCours.resumeTrajet(),
                 m_itineraireEnCours.descriptionEscales(),
                 m_itineraireEnCours.dateDepartInitiale().toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                 prixFormate(totalGeneral)));

    m_lblTarifSieges->setText(
        QStringLiteral("Sièges : <b>%1 / %2</b>  •  Supplément Classe A : <b>+%3</b>  •  Total : <b>%4</b>")
            .arg(choisis.size())
            .arg(nbPax)
            .arg(prixFormate(supplement),
                 prixFormate(totalGeneral)));

    actualiserInstructionsPaiement();
}

void MainWindow::actualiserInstructionsPaiement()
{
    if (!m_lblInstructionsPaiement || !m_comboModePaiement || !m_editContactPaiement)
        return;

    const int nbPax = qMax(1, static_cast<int>(m_blocsPassagers.size()));
    const double baseVols = m_itineraireEnCours.troncons.isEmpty()
                                ? 0.0
                                : (m_itineraireEnCours.prixParPassager * nbPax);
    const double supp = m_planAvion ? m_planAvion->calculerSupplementTotal() : 0.0;
    const QString montant = prixFormate(baseVols + supp);

    const bool estMobile = (m_comboModePaiement->currentIndex() == 0);
    if (estMobile) {
        const QString coordMobile = lireCoordPaiementTicketFly(
            QStringLiteral("mobile_money"),
            QStringLiteral("MVola : 034 00 000 00  |  Orange Money : 032 00 000 00  |  Airtel Money : 033 00 000 00 (TicketFly)"));
        m_lblInstructionsPaiement->setText(
            QStringLiteral("Veuillez envoyer <b>%1</b> vers TicketFly (<b>%2</b>), puis indiquez votre numéro envoyeur et la référence SMS.")
                .arg(montant, coordMobile));
        m_editContactPaiement->setPlaceholderText(QStringLiteral("N° téléphone envoyeur (ex : 034 12 345 67)"));
    } else {
        const QString coordCarte = lireCoordPaiementTicketFly(
            QStringLiteral("carte_bancaire"),
            QStringLiteral("TicketFly — TPE / Compte : 4111 •••• •••• 1234 (BFV-SG / BNI)"));
        m_lblInstructionsPaiement->setText(
            QStringLiteral("Veuillez régler <b>%1</b> par carte sur <b>%2</b>, puis indiquez le titulaire / 4 derniers chiffres et la référence du reçu.")
                .arg(montant, coordCarte));
        m_editContactPaiement->setPlaceholderText(QStringLiteral("Titulaire & 4 derniers chiffres (ex : RAKOTO ••4821)"));
    }
}

void MainWindow::importerPasseportPassager(int indexPassager)
{
    if (indexPassager < 0 || indexPassager >= m_blocsPassagers.size())
        return;

    QString chemin = QFileDialog::getOpenFileName(
        this, QStringLiteral("Sélectionner le passeport du passager %1").arg(indexPassager + 1),
        QDir::homePath(), QStringLiteral("Passeport (*.pdf *.png)"));
    if (chemin.isEmpty())
        return;

    QString numero, nom, prenom, erreur;
    if (!CalculsVol::lireFichierPasseportSansOcr(chemin, numero, nom, prenom, erreur)) {
        m_blocsPassagers[indexPassager].fichierCharge = false;
        m_blocsPassagers[indexPassager].estVerifie = false;
        m_blocsPassagers[indexPassager].lblStatutVerification->setStyleSheet(QStringLiteral("color: #b91c1c;"));
        m_blocsPassagers[indexPassager].lblStatutVerification->setText(erreur);
        return;
    }

    BlocPassagerUI &bloc = m_blocsPassagers[indexPassager];
    bloc.fichierCharge = true;
    bloc.numeroLuDansFichier = numero;
    bloc.nomLuDansFichier = nom;
    bloc.prenomLuDansFichier = prenom;
    verifierConcordancePassager(indexPassager);
}

void MainWindow::verifierConcordancePassager(int indexPassager)
{
    if (indexPassager < 0 || indexPassager >= m_blocsPassagers.size())
        return;

    BlocPassagerUI &bloc = m_blocsPassagers[indexPassager];
    if (!bloc.fichierCharge) {
        bloc.estVerifie = false;
        return;
    }

    bool numOk = (bloc.editNumeroPasseport->text().trimmed().toUpper() == bloc.numeroLuDansFichier);
    bool nomOk = (CalculsVol::normaliserTexteIdentite(bloc.editNom->text())
                  == CalculsVol::normaliserTexteIdentite(bloc.nomLuDansFichier));
    bool preOk = (CalculsVol::normaliserTexteIdentite(bloc.editPrenom->text())
                  == CalculsVol::normaliserTexteIdentite(bloc.prenomLuDansFichier));

    bloc.estVerifie = (numOk && nomOk && preOk);
    if (bloc.estVerifie) {
        bloc.lblStatutVerification->setStyleSheet(QStringLiteral("color: #15803d; font-weight: bold;"));
        bloc.lblStatutVerification->setText(QStringLiteral("Conforme (%1)").arg(bloc.numeroLuDansFichier));
    } else {
        bloc.lblStatutVerification->setStyleSheet(QStringLiteral("color: #b91c1c; font-weight: bold;"));
        bloc.lblStatutVerification->setText(
            QStringLiteral("Non conforme — passeport lu : %1 %2 [%3]")
                .arg(bloc.prenomLuDansFichier, bloc.nomLuDansFichier, bloc.numeroLuDansFichier));
    }
}

void MainWindow::validerDemandeReservation()
{
    if (m_blocsPassagers.isEmpty())
        return;

    const QStringList siegesChoisis = m_planAvion->siegesSelectionnes();
    if (siegesChoisis.size() < m_blocsPassagers.size()) {
        QMessageBox::warning(this, QStringLiteral("Sièges incomplets"),
                             QStringLiteral("Sélectionnez %1 siège(s) sur le plan de l'avion.")
                                 .arg(m_blocsPassagers.size()));
        return;
    }

    QSet<QString> passeportsSaisis;
    QList<PassagerInfo> passagers;

    for (int i = 0; i < m_blocsPassagers.size(); ++i) {
        const BlocPassagerUI &bloc = m_blocsPassagers[i];
        if (bloc.editNom->text().trimmed().size() < 2
            || bloc.editPrenom->text().trimmed().size() < 2
            || bloc.editNumeroPasseport->text().trimmed().size() < 6) {
            QMessageBox::warning(this, QStringLiteral("Passager incomplet"),
                                 QStringLiteral("Complétez tous les champs du Passager %1.").arg(i + 1));
            return;
        }
        if (!bloc.fichierCharge || !bloc.estVerifie) {
            QMessageBox::warning(this, QStringLiteral("Passeport requis"),
                                 QStringLiteral("Importez un passeport conforme pour le Passager %1.").arg(i + 1));
            return;
        }

        QString numNorm = bloc.editNumeroPasseport->text().trimmed().toUpper();
        if (passeportsSaisis.contains(numNorm)) {
            QMessageBox::warning(this, QStringLiteral("Doublon"),
                                 QStringLiteral("Deux passagers ont le même numéro de passeport (%1).").arg(numNorm));
            return;
        }
        passeportsSaisis.insert(numNorm);

        PassagerInfo p;
        p.nom = bloc.editNom->text().trimmed().toUpper();
        p.prenom = bloc.editPrenom->text().trimmed();
        p.dateNaissance = bloc.dateNaissance->date();
        p.numeroPasseport = numNorm;
        p.siege = PlanAvionWidget::estSiegeClasseA(siegesChoisis[i])
                      ? QStringLiteral("%1 (Classe A)").arg(siegesChoisis[i])
                      : siegesChoisis[i];
        passagers.append(p);
    }

    InfosPaiement paiement;
    paiement.modePaiement = m_comboModePaiement->currentText();
    paiement.contactPaiement = m_editContactPaiement->text().trimmed();
    paiement.referencePaiement = m_editReferencePaiement->text().trimmed();

    if (paiement.contactPaiement.size() < 4 || paiement.referencePaiement.size() < 3) {
        QMessageBox::warning(
            this, QStringLiteral("Paiement TicketFly requis"),
            (m_comboModePaiement->currentIndex() == 0)
                ? QStringLiteral("Renseignez le numéro de téléphone envoyeur et la référence du transfert Mobile Money.")
                : QStringLiteral("Renseignez le titulaire / 4 derniers chiffres de la carte et la référence du reçu."));
        return;
    }

    QString reference, erreur;
    QStringList numerosVolsFinaux;
    if (!m_db->creerReservation(m_idUtilisateur, m_itineraireEnCours, passagers,
                                reference, numerosVolsFinaux, erreur,
                                m_planAvion->calculerSupplementTotal(), paiement)) {
        QMessageBox::critical(this, QStringLiteral("Erreur"), erreur);
        return;
    }

    m_editContactPaiement->clear();
    m_editReferencePaiement->clear();
    m_derniereEmpreinteClient = m_db->empreinteReservationsClient(m_idUtilisateur);
    mettreAJourPastilleBoiteReception();

    QMessageBox::information(
        this, QStringLiteral("Réservation envoyée"),
        QStringLiteral("Référence : %1 (En attente de confirmation).\n"
                       "Paiement transmis à l'administration : %2 — Réf. %3.\n"
                       "Un récapitulatif a été ajouté dans votre Boîte de réception.")
            .arg(reference, paiement.modePaiement, paiement.referencePaiement));

    actualiserMesReservations();
    ui->pilePages->setCurrentIndex(PageMesReservations);
}

// ============================================================================
// PAGE 3 : MES RÉSERVATIONS CLIENT
// ============================================================================

void MainWindow::construirePageMesReservations()
{
    auto *layout = new QVBoxLayout(ui->pageMesReservations);

    m_modeleMesReservations = new QSqlQueryModel(this);
    m_tableMesReservations = new QTableView(ui->pageMesReservations);
    m_tableMesReservations->setModel(m_modeleMesReservations);
    configurerTableauStandard(m_tableMesReservations);
    layout->addWidget(m_tableMesReservations, 1);

    m_lblDetailMaReservation = new QLabel(
        QStringLiteral("Sélectionnez une réservation pour voir les détails."), ui->pageMesReservations);
    m_lblDetailMaReservation->setObjectName(QStringLiteral("carteInfo"));
    m_lblDetailMaReservation->setWordWrap(true);
    layout->addWidget(m_lblDetailMaReservation);

    auto *barreActions = new QHBoxLayout();
    auto *btnRetour = creerBouton(QStringLiteral("Retour"), ui->pageMesReservations, "secondaire");
    auto *btnActualiser = creerBoutonActualiserIcone(ui->pageMesReservations);
    m_btnTelechargerPdfClient = creerBouton(QStringLiteral("Télécharger le billet PDF"),
                                            ui->pageMesReservations, "confirmer");
    m_btnTelechargerPdfClient->setEnabled(false);
    auto *btnAnnuler = creerBouton(QStringLiteral("Annuler la réservation"),
                                   ui->pageMesReservations, "danger");

    barreActions->addWidget(btnRetour);
    barreActions->addWidget(btnActualiser);
    barreActions->addStretch();
    barreActions->addWidget(m_btnTelechargerPdfClient);
    barreActions->addWidget(btnAnnuler);
    layout->addLayout(barreActions);

    connect(btnRetour, &QPushButton::clicked, this, [this]() {
        ui->pilePages->setCurrentIndex(PageRecherche);
    });
    connect(btnActualiser, &QPushButton::clicked, this, &MainWindow::actualiserMesReservations);
    connect(m_btnTelechargerPdfClient, &QPushButton::clicked, this, &MainWindow::telechargerMonBilletPdf);
    connect(btnAnnuler, &QPushButton::clicked, this, &MainWindow::annulerMaReservation);
}

void MainWindow::actualiserMesReservations()
{
    if (!m_db || m_idUtilisateur <= 0)
        return;

    m_db->remplirModeleReservationsClient(m_modeleMesReservations, m_idUtilisateur, m_codeMonnaie);
    connect(m_tableMesReservations->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::afficherDetailMaReservation, Qt::UniqueConnection);

    if (m_modeleMesReservations->rowCount() > 0) {
        m_tableMesReservations->selectRow(0);
    } else {
        m_lblDetailMaReservation->setText(QStringLiteral("Aucune réservation enregistrée."));
        m_btnTelechargerPdfClient->setEnabled(false);
    }
}

void MainWindow::afficherDetailMaReservation()
{
    QModelIndex idx = m_tableMesReservations->currentIndex();
    if (!idx.isValid()) {
        m_btnTelechargerPdfClient->setEnabled(false);
        return;
    }

    QString ref = m_modeleMesReservations->data(m_modeleMesReservations->index(idx.row(), 0)).toString();
    DetailsReservation details;
    QString erreur;
    if (!m_db->obtenirDetailsReservation(ref, details, erreur)) {
        m_lblDetailMaReservation->setText(erreur);
        m_btnTelechargerPdfClient->setEnabled(false);
        return;
    }

    m_btnTelechargerPdfClient->setEnabled(details.statut == QStringLiteral("confirmee"));

    QStringList pax, vols;
    for (const PassagerInfo &p : details.passagers)
        pax << QStringLiteral("%1 %2 (Siège %3)").arg(p.prenom, p.nom, p.siege);
    for (const TronconVol &t : details.troncons)
        vols << QStringLiteral("%1 : %2 → %3 (%4)")
                    .arg(t.numeroVol, t.depart.codeIata, t.arrivee.codeIata,
                         t.dateDepart.toString(QStringLiteral("dd/MM HH:mm")));

    const QString infoPaiement = details.referencePaiement.isEmpty()
        ? QString()
        : QStringLiteral("  •  Paiement : <b>%1 (%2 — Réf %3)</b>")
              .arg(details.modePaiement, details.contactPaiement, details.referencePaiement);

    m_lblDetailMaReservation->setText(
        QStringLiteral("<b>%1</b>  •  Statut : <b>%2</b>  •  Total : <b>%3</b>%4<br>Vols : %5  |  Passagers : %6")
            .arg(details.reference, traduireStatutReservation(details.statut),
                 prixFormate(details.prixTotal),
                 infoPaiement,
                 vols.join(QStringLiteral(" + ")),
                 pax.join(QStringLiteral(", "))));
}

void MainWindow::telechargerMonBilletPdf()
{
    QModelIndex idx = m_tableMesReservations->currentIndex();
    if (!idx.isValid())
        return;

    QString ref = m_modeleMesReservations->data(m_modeleMesReservations->index(idx.row(), 0)).toString();
    DetailsReservation details;
    QString erreur;
    if (!m_db->obtenirDetailsReservation(ref, details, erreur)
        || details.statut != QStringLiteral("confirmee")) {
        return;
    }

    QString cheminCible = QFileDialog::getSaveFileName(
        this, QStringLiteral("Enregistrer votre billet PDF"),
        QStringLiteral("Billet_%1.pdf").arg(ref), QStringLiteral("Fichier PDF (*.pdf)"));
    if (cheminCible.isEmpty())
        return;

    if (!BilletPdf::genererBilletUnique(details, cheminCible, erreur)) {
        QMessageBox::critical(this, QStringLiteral("Erreur PDF"), erreur);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(cheminCible));
}

void MainWindow::annulerMaReservation()
{
    QModelIndex idx = m_tableMesReservations->currentIndex();
    if (!idx.isValid())
        return;

    QString ref = m_modeleMesReservations->data(m_modeleMesReservations->index(idx.row(), 0)).toString();
    if (QMessageBox::question(this, QStringLiteral("Confirmer l'annulation"),
                              QStringLiteral("Voulez-vous annuler la réservation %1 ?").arg(ref))
        != QMessageBox::Yes) {
        return;
    }

    QString erreur;
    if (!m_db->annulerReservationClient(ref, m_idUtilisateur, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Annulation refusée"), erreur);
        return;
    }

    m_derniereEmpreinteClient = m_db->empreinteReservationsClient(m_idUtilisateur);
    mettreAJourPastilleBoiteReception();
    actualiserMesReservations();
}

// ============================================================================
// MODULE D'ASSISTANCE GLOBALE & BOÎTE DE RÉCEPTION ULTRA-SIMPLE
// ============================================================================

void MainWindow::ouvrirDialogueAssistance(const QString &sujetPreselectionne)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Assistance & Signalement"));
    dlg.resize(520, 390);

    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *comboSujet = new QComboBox(&dlg);
    comboSujet->addItems({
        QStringLiteral("Mot de passe oublié / Compte bloqué"),
        QStringLiteral("Modification ou annulation d'une réservation"),
        QStringLiteral("Problème de passeport ou billet PDF"),
        QStringLiteral("Question sur un vol ou un siège"),
        QStringLiteral("Autre signalement / Feedback")
    });
    if (!sujetPreselectionne.isEmpty()) {
        int pos = comboSujet->findText(sujetPreselectionne, Qt::MatchContains);
        if (pos >= 0)
            comboSujet->setCurrentIndex(pos);
    }

    auto *editNom = new QLineEdit(m_idUtilisateur > 0 ? m_nomUtilisateur : QString(), &dlg);
    editNom->setPlaceholderText(QStringLiteral("Votre nom"));
    auto *editEmail = new QLineEdit(m_emailUtilisateur, &dlg);
    editEmail->setPlaceholderText(QStringLiteral("votre.email@domaine.com"));

    form->addRow(QStringLiteral("Sujet :"), comboSujet);
    form->addRow(QStringLiteral("Nom :"), editNom);
    form->addRow(QStringLiteral("Email :"), editEmail);
    layout->addLayout(form);

    auto *editCorps = new QPlainTextEdit(&dlg);
    layout->addWidget(editCorps, 1);

    auto regenererBrouillon = [&]() {
        editCorps->setPlainText(
            QStringLiteral("Bonjour,\n\nObjet : %1\nUtilisateur : %2 (%3)\n\n"
                           "Détails de ma demande :\n- ")
                .arg(comboSujet->currentText(),
                     editNom->text().trimmed().isEmpty() ? QStringLiteral("À compléter") : editNom->text().trimmed(),
                     editEmail->text().trimmed().isEmpty() ? QStringLiteral("À compléter") : editEmail->text().trimmed()));
    };
    regenererBrouillon();
    connect(comboSujet, &QComboBox::currentIndexChanged, &dlg, [&]() { regenererBrouillon(); });

    auto *barreBtns = new QHBoxLayout();
    auto *btnMailExterne = creerBouton(QStringLiteral("Ouvrir messagerie mail"), &dlg, "secondaire");
    auto *btnFermer = creerBouton(QStringLiteral("Annuler"), &dlg, "danger");
    auto *btnEnvoyer = creerBouton(QStringLiteral("Envoyer au support"), &dlg, "confirmer");

    barreBtns->addWidget(btnMailExterne);
    barreBtns->addStretch();
    barreBtns->addWidget(btnFermer);
    barreBtns->addWidget(btnEnvoyer);
    layout->addLayout(barreBtns);

    connect(btnFermer, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnMailExterne, &QPushButton::clicked, &dlg, [&]() {
        QUrl url(QStringLiteral("mailto:support@billets-avion.mg"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("subject"), comboSujet->currentText());
        q.addQueryItem(QStringLiteral("body"), editCorps->toPlainText());
        url.setQuery(q);
        QDesktopServices::openUrl(url);
    });

    connect(btnEnvoyer, &QPushButton::clicked, &dlg, [&]() {
        if (editEmail->text().trimmed().isEmpty() || editCorps->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("Champs requis"),
                                 QStringLiteral("Indiquez votre email et votre message."));
            return;
        }

        QString sujet = comboSujet->currentText();
        QString erreurBd;
        if (!m_db->envoyerMessageBoite(0, QStringLiteral("admin"), m_idUtilisateur,
                                       editNom->text().trimmed(), editEmail->text().trimmed(),
                                       QStringLiteral("Assistance"), sujet,
                                       editCorps->toPlainText(), erreurBd)) {
            QMessageBox::critical(&dlg, QStringLiteral("Erreur"), erreurBd);
            return;
        }

        // Confirmation immédiate dans la boîte du client connecté pour qu'il ait la trace de sa demande
        if (m_idUtilisateur > 0) {
            QString ignoreErr;
            m_db->envoyerMessageBoite(
                m_idUtilisateur, QStringLiteral("client"), 0,
                QStringLiteral("Support Client"), QStringLiteral("support@billets-avion.mg"),
                QStringLiteral("Assistance"),
                QStringLiteral("Demande d'assistance enregistrée : %1").arg(sujet),
                QStringLiteral("Votre message a bien été transmis aux administrateurs.\nVous recevrez leur réponse ici."),
                ignoreErr);
        }

        ConfigSmtp cfg = SmtpClient::chargerConfiguration();
        if (cfg.estConfigure()) {
            QString dest = cfg.emailExpediteur.contains(QLatin1Char('@'))
                               ? cfg.emailExpediteur : editEmail->text().trimmed();
            QString errSmtp;
            SmtpClient::envoyerEmailTexteSimple(cfg, dest, sujet, editCorps->toPlainText(), errSmtp);
        }

        mettreAJourPastilleBoiteReception();
        QMessageBox::information(&dlg, QStringLiteral("Envoyé"),
                                 QStringLiteral("Votre demande a été transmise au support."));
        dlg.accept();
    });

    dlg.exec();
}

void MainWindow::construirePageBoiteReception()
{
    auto *layout = new QVBoxLayout(ui->pageBoiteReception);

    // Barre d'actions simple et aérée tout en haut
    auto *barreHaut = new QHBoxLayout();
    m_chkSeulementNonLus = new QCheckBox(QStringLiteral("Non lus uniquement"), ui->pageBoiteReception);
    auto *btnActualiser = creerBoutonActualiserIcone(ui->pageBoiteReception);
    auto *btnToutLu = creerBouton(QStringLiteral("Tout marquer lu"), ui->pageBoiteReception);
    auto *btnSupprimer = creerBouton(QStringLiteral("Supprimer"), ui->pageBoiteReception, "avertissement");
    auto *btnVider = creerBouton(QStringLiteral("Vider"), ui->pageBoiteReception, "danger");
    auto *btnRetour = creerBouton(QStringLiteral("Retour"), ui->pageBoiteReception, "secondaire");

    barreHaut->addWidget(m_chkSeulementNonLus);
    barreHaut->addStretch();
    barreHaut->addWidget(btnActualiser);
    barreHaut->addWidget(btnToutLu);
    barreHaut->addWidget(btnSupprimer);
    barreHaut->addWidget(btnVider);
    barreHaut->addWidget(btnRetour);
    layout->addLayout(barreHaut);

    // Liste pleine largeur placée directement en haut (lisible d'un seul coup d'œil)
    m_listeMessagesBoite = new QListWidget(ui->pageBoiteReception);
    m_listeMessagesBoite->setAlternatingRowColors(true);
    layout->addWidget(m_listeMessagesBoite, 3);

    // Carte claire de lecture juste en dessous
    m_lblLectureMessage = new QLabel(
        QStringLiteral("Cliquez sur une notification ci-dessus pour lire son détail."),
        ui->pageBoiteReception);
    m_lblLectureMessage->setObjectName(QStringLiteral("carteInfo"));
    m_lblLectureMessage->setWordWrap(true);
    m_lblLectureMessage->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_lblLectureMessage->setMinimumHeight(110);
    layout->addWidget(m_lblLectureMessage, 2);

    // Barre compacte de réponse rapide en 1 ligne (visible uniquement côté Admin sur message client)
    m_blocReponseAdmin = new QWidget(ui->pageBoiteReception);
    auto *layoutRep = new QHBoxLayout(m_blocReponseAdmin);
    layoutRep->setContentsMargins(0, 2, 0, 0);
    m_editReponseAdmin = new QLineEdit(m_blocReponseAdmin);
    m_editReponseAdmin->setPlaceholderText(QStringLiteral("Répondre rapidement à ce client..."));
    auto *btnEnvoyerRep = creerBouton(QStringLiteral("Envoyer la réponse"), m_blocReponseAdmin, "confirmer");
    layoutRep->addWidget(m_editReponseAdmin, 1);
    layoutRep->addWidget(btnEnvoyerRep);
    m_blocReponseAdmin->hide();
    layout->addWidget(m_blocReponseAdmin);

    connect(m_chkSeulementNonLus, &QCheckBox::toggled, this, &MainWindow::actualiserBoiteReception);
    connect(btnActualiser, &QPushButton::clicked, this, &MainWindow::actualiserBoiteReception);
    connect(btnToutLu, &QPushButton::clicked, this, [this]() {
        if (m_idUtilisateur > 0) {
            m_db->toutMarquerCommeLu(m_idUtilisateur, m_role);
            actualiserBoiteReception();
        }
    });
    connect(btnSupprimer, &QPushButton::clicked, this, [this]() {
        int row = m_listeMessagesBoite->currentRow();
        if (row >= 0 && row < m_messagesBoite.size()) {
            m_db->supprimerMessageBoite(m_messagesBoite[row].idMessage);
            actualiserBoiteReception();
        }
    });
    connect(btnVider, &QPushButton::clicked, this, [this]() {
        if (m_idUtilisateur > 0) {
            m_db->viderBoiteReception(m_idUtilisateur, m_role);
            actualiserBoiteReception();
        }
    });
    connect(btnRetour, &QPushButton::clicked, this, [this]() {
        ui->pilePages->setCurrentIndex(PageRecherche);
    });
    connect(m_listeMessagesBoite, &QListWidget::currentRowChanged,
            this, &MainWindow::lireMessageSelectionne);
    connect(btnEnvoyerRep, &QPushButton::clicked, this, &MainWindow::repondreAuClientDepuisBoite);
    connect(m_editReponseAdmin, &QLineEdit::returnPressed, this, &MainWindow::repondreAuClientDepuisBoite);
}

void MainWindow::actualiserBoiteReception()
{
    if (!m_db || m_idUtilisateur <= 0)
        return;

    m_listeMessagesBoite->blockSignals(true);
    m_listeMessagesBoite->clear();
    m_messagesBoite = m_db->chargerMessagesBoite(
        m_idUtilisateur, m_role, m_chkSeulementNonLus->isChecked());

    for (const MessageBoiteReception &m : m_messagesBoite) {
        QString etat = m.estLu ? QStringLiteral("✓  ") : QStringLiteral("●  ");
        QString ligne = QStringLiteral("%1%2   —   %3 (%4)")
                            .arg(etat, m.sujet, m.nomExpediteur,
                                 m.dateEnvoi.toString(QStringLiteral("dd/MM HH:mm")));
        auto *item = new QListWidgetItem(ligne, m_listeMessagesBoite);
        if (!m.estLu) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
    }
    m_listeMessagesBoite->blockSignals(false);

    if (!m_messagesBoite.isEmpty()) {
        m_listeMessagesBoite->setCurrentRow(0);
    } else {
        m_lblLectureMessage->setText(QStringLiteral("Aucune notification dans votre boîte."));
        m_blocReponseAdmin->hide();
    }

    m_dernierNbNonLusConnu = m_db->compterMessagesNonLus(m_idUtilisateur, m_role);
    mettreAJourPastilleBoiteReception();
}

void MainWindow::lireMessageSelectionne()
{
    int row = m_listeMessagesBoite->currentRow();
    if (row < 0 || row >= m_messagesBoite.size()) {
        m_blocReponseAdmin->hide();
        return;
    }

    MessageBoiteReception &m = m_messagesBoite[row];
    if (!m.estLu) {
        m_db->marquerMessageCommeLu(m.idMessage);
        m.estLu = true;
        if (QListWidgetItem *it = m_listeMessagesBoite->item(row)) {
            QFont f = it->font();
            f.setBold(false);
            it->setFont(f);
            if (it->text().startsWith(QStringLiteral("●")))
                it->setText(QStringLiteral("✓") + it->text().mid(1));
        }
        m_dernierNbNonLusConnu = m_db->compterMessagesNonLus(m_idUtilisateur, m_role);
        mettreAJourPastilleBoiteReception();
    }

    QString corpsHtml = m.contenu.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    m_lblLectureMessage->setText(
        QStringLiteral("<b>%1</b> &nbsp;—&nbsp; <i>%2 (%3)</i><br><br>%4")
            .arg(m.sujet.toHtmlEscaped(),
                 m.nomExpediteur.toHtmlEscaped(),
                 m.dateEnvoi.toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                 corpsHtml));

    const bool estAdmin = (m_role == QStringLiteral("admin") || m_role == QStringLiteral("super_admin"));
    m_blocReponseAdmin->setVisible(estAdmin && m.idExpediteur > 0);
}

void MainWindow::repondreAuClientDepuisBoite()
{
    int row = m_listeMessagesBoite->currentRow();
    if (row < 0 || row >= m_messagesBoite.size())
        return;

    const MessageBoiteReception &origine = m_messagesBoite[row];
    QString reponse = m_editReponseAdmin->text().trimmed();
    if (origine.idExpediteur <= 0 || reponse.isEmpty())
        return;

    QString erreur;
    if (!m_db->envoyerMessageBoite(
            origine.idExpediteur, QStringLiteral("client"), m_idUtilisateur,
            m_nomUtilisateur + QStringLiteral(" (Support)"), m_emailUtilisateur,
            QStringLiteral("Assistance"),
            QStringLiteral("Réponse du support : ") + origine.sujet,
            reponse, erreur)) {
        QMessageBox::critical(this, QStringLiteral("Erreur"), erreur);
        return;
    }

    m_editReponseAdmin->clear();
    statusBar()->showMessage(QStringLiteral("Réponse envoyée dans la boîte du client."), 3500);
}

// ============================================================================
// PAGE 4 : ESPACE ADMINISTRATION & SUPER ADMINISTRATION
// ============================================================================

void MainWindow::construirePageAdmin()
{
    auto *layoutPage = new QVBoxLayout(ui->pageAdmin);
    m_ongletsAdmin = new QTabWidget(ui->pageAdmin);
    layoutPage->addWidget(m_ongletsAdmin);

    auto *validateurNom = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[\\p{L} '\\-]{2,80}$")), this);
    auto *validateurEmailSaisie = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[^\\s+]{0,140}$")), this);

    // 1. Tableau de bord
    auto *tabDashboard = new QWidget(m_ongletsAdmin);
    auto *layoutDash = new QVBoxLayout(tabDashboard);

    m_lblStatsResume = new QLabel(tabDashboard);
    m_lblStatsResume->setObjectName(QStringLiteral("carteInfo"));
    m_lblStatsResume->setWordWrap(true);
    layoutDash->addWidget(m_lblStatsResume);

    auto *groupeGraphique = new QGroupBox(QStringLiteral("Répartition par statut"), tabDashboard);
    auto *layoutGraphique = new QVBoxLayout(groupeGraphique);
    m_graphiqueStatuts = new GraphiqueStatutsWidget(groupeGraphique);
    layoutGraphique->addWidget(m_graphiqueStatuts);
    layoutDash->addWidget(groupeGraphique);

    auto *barreDash = new QHBoxLayout();
    auto *btnMajDash = creerBoutonActualiserIcone(tabDashboard);
    auto *btnExportDash = creerBouton(QStringLiteral("Exporter les réservations en CSV"), tabDashboard);
    barreDash->addWidget(btnMajDash);
    barreDash->addWidget(btnExportDash);
    barreDash->addStretch();
    layoutDash->addLayout(barreDash);
    layoutDash->addStretch();

    connect(btnMajDash, &QPushButton::clicked, this, &MainWindow::actualiserEspaceAdmin);
    connect(btnExportDash, &QPushButton::clicked, this, &MainWindow::exporterCsvAdmin);
    m_ongletsAdmin->addTab(tabDashboard, QStringLiteral("Tableau de bord"));

    // 2. Réservations
    auto *tabRes = new QWidget(m_ongletsAdmin);
    auto *layoutRes = new QVBoxLayout(tabRes);

    auto *barreFiltreRes = new QHBoxLayout();
    barreFiltreRes->addWidget(new QLabel(QStringLiteral("Statut :"), tabRes));
    m_filtreStatutAdmin = new QComboBox(tabRes);
    m_filtreStatutAdmin->addItem(QStringLiteral("Tous"), QString());
    m_filtreStatutAdmin->addItem(QStringLiteral("En attente"), QStringLiteral("en_attente"));
    m_filtreStatutAdmin->addItem(QStringLiteral("Confirmée"), QStringLiteral("confirmee"));
    m_filtreStatutAdmin->addItem(QStringLiteral("Annulée"), QStringLiteral("annulee"));
    auto *btnActuRes = creerBoutonActualiserIcone(tabRes);
    barreFiltreRes->addWidget(m_filtreStatutAdmin);
    barreFiltreRes->addWidget(btnActuRes);
    barreFiltreRes->addStretch();
    layoutRes->addLayout(barreFiltreRes);

    m_modeleResAdmin = new QSqlQueryModel(this);
    m_tableResAdmin = new QTableView(tabRes);
    m_tableResAdmin->setModel(m_modeleResAdmin);
    configurerTableauStandard(m_tableResAdmin);
    layoutRes->addWidget(m_tableResAdmin, 1);

    m_lblDetailResAdmin = new QLabel(tabRes);
    m_lblDetailResAdmin->setObjectName(QStringLiteral("carteInfo"));
    m_lblDetailResAdmin->setWordWrap(true);
    layoutRes->addWidget(m_lblDetailResAdmin);

    auto *barreStatuts = new QHBoxLayout();
    auto *btnConfirmer = creerBouton(QStringLiteral("Confirmer (Capacité + PDF + Email)"), tabRes, "confirmer");
    auto *btnAttente = creerBouton(QStringLiteral("Mettre en attente"), tabRes, "avertissement");
    auto *btnAnnuler = creerBouton(QStringLiteral("Annuler"), tabRes, "danger");
    auto *btnPdfAdmin = creerBouton(QStringLiteral("Billet PDF"), tabRes, "secondaire");
    auto *btnCsvAdmin = creerBouton(QStringLiteral("CSV"), tabRes, "secondaire");

    barreStatuts->addWidget(btnConfirmer);
    barreStatuts->addWidget(btnAttente);
    barreStatuts->addWidget(btnAnnuler);
    barreStatuts->addStretch();
    barreStatuts->addWidget(btnPdfAdmin);
    barreStatuts->addWidget(btnCsvAdmin);
    layoutRes->addLayout(barreStatuts);

    connect(m_filtreStatutAdmin, &QComboBox::currentIndexChanged, this, &MainWindow::actualiserEspaceAdmin);
    connect(btnActuRes, &QPushButton::clicked, this, &MainWindow::actualiserEspaceAdmin);
    connect(btnAttente, &QPushButton::clicked, this, [this]() { appliquerStatutAdmin(QStringLiteral("en_attente")); });
    connect(btnConfirmer, &QPushButton::clicked, this, [this]() { appliquerStatutAdmin(QStringLiteral("confirmee")); });
    connect(btnAnnuler, &QPushButton::clicked, this, [this]() { appliquerStatutAdmin(QStringLiteral("annulee")); });
    connect(btnPdfAdmin, &QPushButton::clicked, this, &MainWindow::enregistrerBilletPdfAdmin);
    connect(btnCsvAdmin, &QPushButton::clicked, this, &MainWindow::exporterCsvAdmin);

    m_ongletsAdmin->addTab(tabRes, QStringLiteral("Réservations"));

    // 3. Vols programmés
    auto *tabVols = new QWidget(m_ongletsAdmin);
    auto *layoutVols = new QVBoxLayout(tabVols);

    m_modeleVolsAdmin = new QSqlQueryModel(this);
    m_tableVolsAdmin = new QTableView(tabVols);
    m_tableVolsAdmin->setModel(m_modeleVolsAdmin);
    configurerTableauStandard(m_tableVolsAdmin);
    layoutVols->addWidget(m_tableVolsAdmin, 1);

    auto *groupeAjoutVol = new QGroupBox(QStringLiteral("Programmer ou modifier un vol"), tabVols);
    auto *formVol = new QFormLayout(groupeAjoutVol);

    m_comboVolDepAdmin = new QComboBox(groupeAjoutVol);
    m_comboVolArrAdmin = new QComboBox(groupeAjoutVol);
    m_editCompagnieAdmin = new QLineEdit(QStringLiteral("Madagascar Airlines"), groupeAjoutVol);
    m_dateHeureVolAdmin = new QDateTimeEdit(QDateTime::currentDateTime().addDays(2), groupeAjoutVol);
    m_dateHeureVolAdmin->setCalendarPopup(true);
    m_dateHeureVolAdmin->setMinimumDateTime(QDateTime::currentDateTime());
    m_dateHeureVolAdmin->setDisplayFormat(QStringLiteral("dd/MM/yyyy HH:mm"));

    m_spinCapaciteAdmin = new QSpinBox(groupeAjoutVol);
    m_spinCapaciteAdmin->setRange(10, 500);
    m_spinCapaciteAdmin->setValue(Constantes::CAPACITE_CABINE_DEFAUT);

    m_spinPrixAdmin = new QDoubleSpinBox(groupeAjoutVol);
    m_spinPrixAdmin->setRange(0.0, 200000000.0);
    m_spinPrixAdmin->setValue(0.0);
    m_spinPrixAdmin->setSuffix(QStringLiteral(" € (0 = auto)"));

    m_chkNotifierModifVol = new QCheckBox(
        QStringLiteral("En cas de modification : notifier les clients réservés et leur envoyer par mail leurs nouveaux billets PDF"),
        groupeAjoutVol);
    m_chkNotifierModifVol->setChecked(true);

    auto *ligneBoutonsVol = new QHBoxLayout();
    auto *btnAjouterVol = creerBouton(QStringLiteral("Programmer ce vol"), groupeAjoutVol, "confirmer");
    auto *btnModifierVol = creerBouton(QStringLiteral("Modifier le vol sélectionné"), groupeAjoutVol, "avertissement");
    auto *btnSupprVol = creerBouton(QStringLiteral("Supprimer le vol sélectionné"), groupeAjoutVol, "danger");
    ligneBoutonsVol->addWidget(btnAjouterVol);
    ligneBoutonsVol->addWidget(btnModifierVol);
    ligneBoutonsVol->addWidget(btnSupprVol);
    ligneBoutonsVol->addStretch();

    formVol->addRow(QStringLiteral("Départ :"), m_comboVolDepAdmin);
    formVol->addRow(QStringLiteral("Arrivée :"), m_comboVolArrAdmin);
    formVol->addRow(QStringLiteral("Compagnie :"), m_editCompagnieAdmin);
    formVol->addRow(QStringLiteral("Départ le :"), m_dateHeureVolAdmin);
    formVol->addRow(QStringLiteral("Capacité :"), m_spinCapaciteAdmin);
    formVol->addRow(QStringLiteral("Tarif :"), m_spinPrixAdmin);
    formVol->addRow(QStringLiteral("Clients :"), m_chkNotifierModifVol);
    formVol->addRow(QString(), ligneBoutonsVol);

    layoutVols->addWidget(groupeAjoutVol);
    connect(btnAjouterVol, &QPushButton::clicked, this, &MainWindow::ajouterVolAdmin);
    connect(btnModifierVol, &QPushButton::clicked, this, &MainWindow::modifierVolAdmin);
    connect(btnSupprVol, &QPushButton::clicked, this, &MainWindow::supprimerVolAdmin);

    m_ongletsAdmin->addTab(tabVols, QStringLiteral("Vols programmés"));

    // 4. Clients & Assistance
    auto *tabClients = new QWidget(m_ongletsAdmin);
    auto *layoutClients = new QVBoxLayout(tabClients);

    auto *barreRechClient = new QHBoxLayout();
    barreRechClient->addWidget(new QLabel(QStringLiteral("Rechercher :"), tabClients));
    m_rechercheClientAdmin = new QLineEdit(tabClients);
    m_rechercheClientAdmin->setPlaceholderText(QStringLiteral("Nom, prénom ou email..."));
    barreRechClient->addWidget(m_rechercheClientAdmin, 1);
    layoutClients->addLayout(barreRechClient);

    m_modeleClientsAdmin = new QSqlQueryModel(this);
    m_tableClientsAdmin = new QTableView(tabClients);
    m_tableClientsAdmin->setModel(m_modeleClientsAdmin);
    configurerTableauStandard(m_tableClientsAdmin);
    layoutClients->addWidget(m_tableClientsAdmin, 1);

    auto *groupeEditClient = new QGroupBox(QStringLiteral("Modifier / Assister un client"), tabClients);
    auto *formClient = new QFormLayout(groupeEditClient);

    m_clientNomEdit = new QLineEdit(groupeEditClient);
    m_clientNomEdit->setValidator(validateurNom);
    m_clientPrenomEdit = new QLineEdit(groupeEditClient);
    m_clientPrenomEdit->setValidator(validateurNom);
    m_clientEmailEdit = new QLineEdit(groupeEditClient);
    m_clientEmailEdit->setValidator(validateurEmailSaisie);
    m_chkClientBloque = new QCheckBox(QStringLiteral("Bloquer ce compte"), groupeEditClient);

    auto *ligneMdpClient = new QHBoxLayout();
    m_clientNouveauMdp = new QLineEdit(groupeEditClient);
    m_clientNouveauMdp->setPlaceholderText(QStringLiteral("Nouveau mot de passe (optionnel)"));
    auto *btnGenererMdp = creerBouton(QStringLiteral("Générer mot de passe"), groupeEditClient, "avertissement");
    ligneMdpClient->addWidget(m_clientNouveauMdp, 1);
    ligneMdpClient->addWidget(btnGenererMdp);

    auto *ligneBoutonsClient = new QHBoxLayout();
    auto *btnSauverClient = creerBouton(QStringLiteral("Enregistrer le client"), groupeEditClient, "confirmer");
    auto *btnSupprClient = creerBouton(QStringLiteral("Supprimer ce client"), groupeEditClient, "danger");
    ligneBoutonsClient->addWidget(btnSauverClient);
    ligneBoutonsClient->addWidget(btnSupprClient);
    ligneBoutonsClient->addStretch();

    formClient->addRow(QStringLiteral("Nom :"), m_clientNomEdit);
    formClient->addRow(QStringLiteral("Prénom :"), m_clientPrenomEdit);
    formClient->addRow(QStringLiteral("Email :"), m_clientEmailEdit);
    formClient->addRow(QStringLiteral("Accès :"), m_chkClientBloque);
    formClient->addRow(QStringLiteral("Mot de passe :"), ligneMdpClient);
    formClient->addRow(QString(), ligneBoutonsClient);
    layoutClients->addWidget(groupeEditClient);

    connect(m_rechercheClientAdmin, &QLineEdit::textChanged, this, &MainWindow::actualiserListeClientsAdmin);
    connect(btnGenererMdp, &QPushButton::clicked, this, [this]() {
        int code = QRandomGenerator::global()->bounded(1000, 9999);
        m_clientNouveauMdp->setText(QStringLiteral("Pass%1!").arg(code));
    });
    connect(btnSauverClient, &QPushButton::clicked, this, &MainWindow::modifierClientDepuisAdmin);
    connect(btnSupprClient, &QPushButton::clicked, this, &MainWindow::supprimerClientDepuisAdmin);

    m_ongletsAdmin->addTab(tabClients, QStringLiteral("Clients & Assistance"));

    // 5. Configuration (Paiement TicketFly + Email SMTP)
    auto *tabSmtp = new QWidget(m_ongletsAdmin);
    auto *layoutCfgGlobal = new QVBoxLayout(tabSmtp);
    layoutCfgGlobal->setContentsMargins(20, 14, 20, 14);
    layoutCfgGlobal->setSpacing(12);

    auto *groupeCfgPaiement = new QGroupBox(QStringLiteral("Coordonnées de paiement TicketFly (affichées aux clients)"), tabSmtp);
    auto *formCfgPaiement = new QFormLayout(groupeCfgPaiement);
    m_paiementMobileMoneyCfg = new QLineEdit(
        lireCoordPaiementTicketFly(
            QStringLiteral("mobile_money"),
            QStringLiteral("MVola : 034 00 000 00  |  Orange Money : 032 00 000 00  |  Airtel Money : 033 00 000 00 (TicketFly)")),
        groupeCfgPaiement);
    m_paiementCarteBancaireCfg = new QLineEdit(
        lireCoordPaiementTicketFly(
            QStringLiteral("carte_bancaire"),
            QStringLiteral("TicketFly — TPE / Compte : 4111 •••• •••• 1234 (BFV-SG / BNI)")),
        groupeCfgPaiement);
    formCfgPaiement->addRow(QStringLiteral("Mobile Money :"), m_paiementMobileMoneyCfg);
    formCfgPaiement->addRow(QStringLiteral("Carte bancaire :"), m_paiementCarteBancaireCfg);
    layoutCfgGlobal->addWidget(groupeCfgPaiement);

    auto *groupeCfgSmtp = new QGroupBox(QStringLiteral("Serveur Email SMTP (envoi automatique des billets PDF)"), tabSmtp);
    auto *formSmtp = new QFormLayout(groupeCfgSmtp);

    ConfigSmtp cfg = SmtpClient::chargerConfiguration();
    m_smtpServeur = new QLineEdit(cfg.serveur, groupeCfgSmtp);
    m_smtpPort = new QSpinBox(groupeCfgSmtp);
    m_smtpPort->setRange(1, 65535);
    m_smtpPort->setValue(cfg.port);
    m_smtpIdentifiant = new QLineEdit(cfg.identifiantSmtp, groupeCfgSmtp);
    m_smtpIdentifiant->setPlaceholderText(QStringLiteral("Username Mailtrap (laisser vide pour Gmail)"));
    m_smtpEmail = new QLineEdit(cfg.emailExpediteur, groupeCfgSmtp);
    m_smtpMdpApp = new QLineEdit(cfg.motDePasseApplication, groupeCfgSmtp);
    m_smtpMdpApp->setEchoMode(QLineEdit::Password);
    m_smtpNom = new QLineEdit(cfg.nomExpediteur, groupeCfgSmtp);

    formSmtp->addRow(QStringLiteral("Serveur SMTP :"), m_smtpServeur);
    formSmtp->addRow(QStringLiteral("Port :"), m_smtpPort);
    formSmtp->addRow(QStringLiteral("Identifiant (optionnel) :"), m_smtpIdentifiant);
    formSmtp->addRow(QStringLiteral("Email expéditeur :"), m_smtpEmail);
    formSmtp->addRow(QStringLiteral("Mot de passe :"), m_smtpMdpApp);
    formSmtp->addRow(QStringLiteral("Nom affiché :"), m_smtpNom);
    layoutCfgGlobal->addWidget(groupeCfgSmtp);

    auto *btnSauverSmtp = creerBouton(QStringLiteral("Enregistrer la configuration (Paiement & SMTP)"), tabSmtp, "confirmer");
    connect(btnSauverSmtp, &QPushButton::clicked, this, &MainWindow::sauvegarderConfigSmtpAdmin);
    layoutCfgGlobal->addWidget(btnSauverSmtp, 0, Qt::AlignLeft);
    layoutCfgGlobal->addStretch();

    m_ongletsAdmin->addTab(tabSmtp, QStringLiteral("Configuration"));

    // 6. Super Admin — CRUD complet des administrateurs
    m_ongletSuperAdminWidget = new QWidget(m_ongletsAdmin);
    auto *layoutSuper = new QVBoxLayout(m_ongletSuperAdminWidget);

    m_modeleAdmins = new QSqlQueryModel(this);
    m_tableAdmins = new QTableView(m_ongletSuperAdminWidget);
    m_tableAdmins->setModel(m_modeleAdmins);
    configurerTableauStandard(m_tableAdmins);
    layoutSuper->addWidget(m_tableAdmins, 1);

    auto *groupeCrudAdmin = new QGroupBox(QStringLiteral("Gérer les administrateurs"), m_ongletSuperAdminWidget);
    auto *formAdmin = new QFormLayout(groupeCrudAdmin);

    m_adminNom = new QLineEdit(groupeCrudAdmin);
    m_adminNom->setValidator(validateurNom);
    m_adminPrenom = new QLineEdit(groupeCrudAdmin);
    m_adminPrenom->setValidator(validateurNom);
    m_adminEmail = new QLineEdit(groupeCrudAdmin);
    m_adminEmail->setValidator(validateurEmailSaisie);
    m_adminMdp = new QLineEdit(groupeCrudAdmin);
    m_adminMdp->setEchoMode(QLineEdit::Password);
    m_adminMdp->setPlaceholderText(QStringLiteral("Obligatoire en création, optionnel en modification"));

    auto *boutonsSuper = new QHBoxLayout();
    auto *btnCreerAdmin = creerBouton(QStringLiteral("Créer"), groupeCrudAdmin, "confirmer");
    auto *btnModifierAdmin = creerBouton(QStringLiteral("Modifier"), groupeCrudAdmin);
    auto *btnSupprAdmin = creerBouton(QStringLiteral("Supprimer"), groupeCrudAdmin, "danger");
    boutonsSuper->addWidget(btnCreerAdmin);
    boutonsSuper->addWidget(btnModifierAdmin);
    boutonsSuper->addWidget(btnSupprAdmin);
    boutonsSuper->addStretch();

    formAdmin->addRow(QStringLiteral("Nom :"), m_adminNom);
    formAdmin->addRow(QStringLiteral("Prénom :"), m_adminPrenom);
    formAdmin->addRow(QStringLiteral("Email :"), m_adminEmail);
    formAdmin->addRow(QStringLiteral("Mot de passe :"), m_adminMdp);
    formAdmin->addRow(QString(), boutonsSuper);
    layoutSuper->addWidget(groupeCrudAdmin);

    connect(btnCreerAdmin, &QPushButton::clicked, this, &MainWindow::creerCompteAdminParSuperAdmin);
    connect(btnModifierAdmin, &QPushButton::clicked, this, &MainWindow::modifierCompteAdminParSuperAdmin);
    connect(btnSupprAdmin, &QPushButton::clicked, this, &MainWindow::supprimerCompteAdminParSuperAdmin);

    m_indexOngletSuperAdmin = m_ongletsAdmin->addTab(
        m_ongletSuperAdminWidget, QStringLiteral("Super Admin"));
}

void MainWindow::actualiserEspaceAdmin()
{
    if (!m_db)
        return;

    m_comboVolDepAdmin->clear();
    m_comboVolArrAdmin->clear();
    for (const AeroportInfo &a : m_aeroports) {
        QString lib = QStringLiteral("%1 (%2) — %3").arg(a.ville, a.codeIata, a.pays);
        m_comboVolDepAdmin->addItem(lib, a.idAeroport);
        m_comboVolArrAdmin->addItem(lib, a.idAeroport);
    }
    if (m_comboVolArrAdmin->count() > 1)
        m_comboVolArrAdmin->setCurrentIndex(1);

    StatistiquesAdmin s = m_db->obtenirStatistiquesAdmin();
    m_lblStatsResume->setText(
        QStringLiteral("<b>Vols actifs :</b> %1   |   <b>Réservations :</b> %2   |   "
                       "<b>En attente :</b> %3   |   <b>Confirmées :</b> %4   |   "
                       "<b>Annulées :</b> %5   |   <b>CA :</b> %6")
            .arg(s.nbVolsProgrammes).arg(s.totalReservations)
            .arg(s.nbEnAttente).arg(s.nbConfirmees).arg(s.nbAnnulees)
            .arg(prixFormate(s.chiffreAffairesConfirme)));

    static_cast<GraphiqueStatutsWidget *>(m_graphiqueStatuts)
        ->definirValeurs(s.nbEnAttente, s.nbConfirmees, s.nbAnnulees);

    m_db->remplirModeleReservationsAdmin(
        m_modeleResAdmin, m_filtreStatutAdmin->currentData().toString(), m_codeMonnaie);
    connect(m_tableResAdmin->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::afficherDetailReservationAdmin, Qt::UniqueConnection);
    if (m_modeleResAdmin->rowCount() > 0)
        m_tableResAdmin->selectRow(0);

    m_db->remplirModeleVolsAdmin(m_modeleVolsAdmin, m_codeMonnaie);
    m_tableVolsAdmin->setColumnHidden(0, true); // Cache la colonne technique id_vol
    connect(m_tableVolsAdmin->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::chargerVolSelectionneAdmin, Qt::UniqueConnection);
    actualiserListeClientsAdmin();

    if (m_role == QStringLiteral("super_admin")) {
        m_db->remplirModeleAdministrateurs(m_modeleAdmins);
        connect(m_tableAdmins->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &MainWindow::chargerAdminSelectionneSuperAdmin, Qt::UniqueConnection);
    }

    mettreAJourPastilleBoiteReception();
}

void MainWindow::afficherDetailReservationAdmin()
{
    QModelIndex idx = m_tableResAdmin->currentIndex();
    if (!idx.isValid())
        return;

    QString ref = m_modeleResAdmin->data(m_modeleResAdmin->index(idx.row(), 0)).toString();
    DetailsReservation d;
    QString erreur;
    if (!m_db->obtenirDetailsReservation(ref, d, erreur)) {
        m_lblDetailResAdmin->setText(erreur);
        return;
    }

    QStringList pax, vols;
    for (const PassagerInfo &p : d.passagers)
        pax << QStringLiteral("%1 %2 [%3 — %4]").arg(p.prenom, p.nom, p.numeroPasseport, p.siege);
    for (const TronconVol &t : d.troncons)
        vols << QStringLiteral("%1 (%2→%3, %4 pl.)")
                    .arg(t.numeroVol, t.depart.codeIata, t.arrivee.codeIata,
                         QString::number(t.placesDisponibles));

    const QString lignePaiement = d.referencePaiement.isEmpty()
        ? QStringLiteral("Non renseigné")
        : QStringLiteral("%1 — Envoyeur/Carte : <b>%2</b> — Réf : <b>%3</b>")
              .arg(d.modePaiement, d.contactPaiement, d.referencePaiement);

    m_lblDetailResAdmin->setText(
        QStringLiteral("<b>%1</b> — %2 %3 (%4) — <b>%5</b> — <b>%6</b>"
                       "<br>Paiement TicketFly : %7"
                       "<br>Vols : %8  |  Passagers : %9")
            .arg(d.reference, d.prenomClient, d.nomClient, d.emailClient,
                 traduireStatutReservation(d.statut),
                 prixFormate(d.prixTotal),
                 lignePaiement,
                 vols.join(QStringLiteral(" ; ")),
                 pax.join(QStringLiteral(" | "))));
}

void MainWindow::appliquerStatutAdmin(const QString &nouveauStatut)
{
    QModelIndex idx = m_tableResAdmin->currentIndex();
    if (!idx.isValid())
        return;

    QString ref = m_modeleResAdmin->data(m_modeleResAdmin->index(idx.row(), 0)).toString();
    QString erreur;
    if (!m_db->changerStatutReservation(ref, nouveauStatut, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Modification impossible"), erreur);
        return;
    }

    if (nouveauStatut == QStringLiteral("confirmee")) {
        DetailsReservation details;
        if (m_db->obtenirDetailsReservation(ref, details, erreur)) {
            QString dossierPdf = QCoreApplication::applicationDirPath() + QStringLiteral("/billets_generes");
            QDir().mkpath(dossierPdf);
            QString cheminPdf = dossierPdf + QStringLiteral("/Billet_%1.pdf").arg(ref);

            QString errPdf;
            if (BilletPdf::genererBilletUnique(details, cheminPdf, errPdf)) {
                ConfigSmtp cfgSmtp = SmtpClient::chargerConfiguration();
                QString errSmtp;
                bool mailOk = SmtpClient::envoyerEmailConfirmation(cfgSmtp, details, cheminPdf, errSmtp);
                QMessageBox::information(
                    this, QStringLiteral("Réservation confirmée"),
                    mailOk ? QStringLiteral("Billet PDF généré et envoyé par email à %1.").arg(details.emailClient)
                           : QStringLiteral("Billet PDF généré (%1).\nNotification déposée dans la Boîte de réception du client.\nInfo SMTP : %2")
                                 .arg(cheminPdf, errSmtp));
            }
        }
    }

    actualiserEspaceAdmin();
    executerRechercheOuAffichageComplet(false);
}

void MainWindow::enregistrerBilletPdfAdmin()
{
    QModelIndex idx = m_tableResAdmin->currentIndex();
    if (!idx.isValid())
        return;

    QString ref = m_modeleResAdmin->data(m_modeleResAdmin->index(idx.row(), 0)).toString();
    DetailsReservation details;
    QString erreur;
    if (!m_db->obtenirDetailsReservation(ref, details, erreur))
        return;

    QString chemin = QFileDialog::getSaveFileName(
        this, QStringLiteral("Enregistrer le billet PDF"),
        QStringLiteral("Billet_%1.pdf").arg(ref), QStringLiteral("Fichier PDF (*.pdf)"));
    if (chemin.isEmpty())
        return;

    if (BilletPdf::genererBilletUnique(details, chemin, erreur))
        QDesktopServices::openUrl(QUrl::fromLocalFile(chemin));
}

void MainWindow::exporterCsvAdmin()
{
    QString chemin = QFileDialog::getSaveFileName(
        this, QStringLiteral("Exporter en CSV"),
        QStringLiteral("reservations_export.csv"), QStringLiteral("Fichier CSV (*.csv)"));
    if (chemin.isEmpty())
        return;

    QString erreur;
    if (!m_db->exporterReservationsCsv(chemin, erreur))
        QMessageBox::critical(this, QStringLiteral("Erreur CSV"), erreur);
    else
        statusBar()->showMessage(QStringLiteral("Export CSV enregistré : %1").arg(chemin), 4000);
}

void MainWindow::chargerVolSelectionneAdmin()
{
    QModelIndex idx = m_tableVolsAdmin->currentIndex();
    if (!idx.isValid() || !m_db)
        return;

    const int idVol = m_modeleVolsAdmin->data(m_modeleVolsAdmin->index(idx.row(), 0)).toInt();
    const QString dispoTxt = m_modeleVolsAdmin->data(m_modeleVolsAdmin->index(idx.row(), 9)).toString();
    const int capaciteLue = dispoTxt.section(QLatin1Char('/'), 1, 1).trimmed().toInt();

    for (const TronconVol &v : m_db->chargerVolsProgrammes()) {
        if (v.idVolExistant == idVol) {
            int iDep = m_comboVolDepAdmin->findData(v.depart.idAeroport);
            int iArr = m_comboVolArrAdmin->findData(v.arrivee.idAeroport);
            if (iDep >= 0) m_comboVolDepAdmin->setCurrentIndex(iDep);
            if (iArr >= 0) m_comboVolArrAdmin->setCurrentIndex(iArr);
            m_editCompagnieAdmin->setText(v.compagnie);
            if (v.dateDepart > QDateTime::currentDateTime())
                m_dateHeureVolAdmin->setDateTime(v.dateDepart);
            if (capaciteLue >= 10)
                m_spinCapaciteAdmin->setValue(capaciteLue);
            m_spinPrixAdmin->setValue(v.prixUnitaire * Constantes::tauxMonnaieDepuisEur(m_codeMonnaie));
            break;
        }
    }
}

void MainWindow::ajouterVolAdmin()
{
    int idDep = m_comboVolDepAdmin->currentData().toInt();
    int idArr = m_comboVolArrAdmin->currentData().toInt();
    double prixEur = m_spinPrixAdmin->value() / Constantes::tauxMonnaieDepuisEur(m_codeMonnaie);

    QString numeroCree, erreur;
    if (!m_db->ajouterVolManuel(m_editCompagnieAdmin->text(), idDep, idArr,
                                m_dateHeureVolAdmin->dateTime(), m_spinCapaciteAdmin->value(),
                                prixEur, numeroCree, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Impossible de programmer"), erreur);
        return;
    }

    actualiserEspaceAdmin();
    executerRechercheOuAffichageComplet(false);
    statusBar()->showMessage(QStringLiteral("Vol %1 programmé.").arg(numeroCree), 4000);
}

void MainWindow::modifierVolAdmin()
{
    QModelIndex idx = m_tableVolsAdmin->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, QStringLiteral("Sélection requise"),
                                 QStringLiteral("Sélectionnez d'abord un vol dans le tableau."));
        return;
    }

    const int idVol = m_modeleVolsAdmin->data(m_modeleVolsAdmin->index(idx.row(), 0)).toInt();
    const QString numeroVol = m_modeleVolsAdmin->data(m_modeleVolsAdmin->index(idx.row(), 1)).toString();
    const int idDep = m_comboVolDepAdmin->currentData().toInt();
    const int idArr = m_comboVolArrAdmin->currentData().toInt();
    const double prixEur = m_spinPrixAdmin->value() / Constantes::tauxMonnaieDepuisEur(m_codeMonnaie);
    const bool notifier = m_chkNotifierModifVol && m_chkNotifierModifVol->isChecked();

    QStringList refsImpactees;
    QString erreur;
    if (!m_db->modifierVolManuel(idVol, m_editCompagnieAdmin->text(), idDep, idArr,
                                 m_dateHeureVolAdmin->dateTime(), m_spinCapaciteAdmin->value(),
                                 prixEur, notifier, refsImpactees, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Modification refusée"), erreur);
        return;
    }

    // Régénération des billets PDF mis à jour et envoi par email si demandé
    int nbMailsEnvoyes = 0;
    ConfigSmtp cfgSmtp = SmtpClient::chargerConfiguration();
    QString dossierPdf = QCoreApplication::applicationDirPath() + QStringLiteral("/billets_generes");
    QDir().mkpath(dossierPdf);

    for (const QString &ref : refsImpactees) {
        DetailsReservation details;
        QString errDet;
        if (!m_db->obtenirDetailsReservation(ref, details, errDet))
            continue;

        QString cheminPdf = dossierPdf + QStringLiteral("/Billet_%1.pdf").arg(ref);
        QString errPdf;
        if (BilletPdf::genererBilletUnique(details, cheminPdf, errPdf)
            && notifier && cfgSmtp.estConfigure()) {
            QString errMail;
            if (SmtpClient::envoyerEmailConfirmation(cfgSmtp, details, cheminPdf, errMail, true))
                ++nbMailsEnvoyes;
        }
    }

    actualiserEspaceAdmin();
    executerRechercheOuAffichageComplet(false);

    if (refsImpactees.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Vol mis à jour"),
                                 QStringLiteral("Le vol %1 a bien été modifié.").arg(numeroVol));
    } else if (notifier) {
        QMessageBox::information(
            this, QStringLiteral("Vol modifié et clients informés"),
            QStringLiteral("Le vol %1 a été mis à jour.\n"
                           "• %2 réservation(s) impactée(s) notifiée(s) dans la Boîte de réception.\n"
                           "• Nouveaux billets PDF régénérés (%3 envoyé(s) par email).")
                .arg(numeroVol)
                .arg(refsImpactees.size())
                .arg(nbMailsEnvoyes));
    } else {
        QMessageBox::information(
            this, QStringLiteral("Vol mis à jour"),
            QStringLiteral("Le vol %1 a été modifié (%2 réservation(s) rattachée(s), billets PDF régénérés).")
                .arg(numeroVol).arg(refsImpactees.size()));
    }
}

void MainWindow::supprimerVolAdmin()
{
    QModelIndex idx = m_tableVolsAdmin->currentIndex();
    if (!idx.isValid())
        return;

    int idVol = m_modeleVolsAdmin->data(m_modeleVolsAdmin->index(idx.row(), 0)).toInt();
    QString erreur;
    if (!m_db->supprimerVol(idVol, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Suppression refusée"), erreur);
        return;
    }
    actualiserEspaceAdmin();
    executerRechercheOuAffichageComplet(false);
}

void MainWindow::actualiserListeClientsAdmin()
{
    if (!m_db || !m_modeleClientsAdmin)
        return;

    m_db->remplirModeleClientsAdmin(
        m_modeleClientsAdmin, m_rechercheClientAdmin ? m_rechercheClientAdmin->text() : QString());
    connect(m_tableClientsAdmin->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::chargerClientSelectionneAdmin, Qt::UniqueConnection);
    if (m_modeleClientsAdmin->rowCount() > 0)
        m_tableClientsAdmin->selectRow(0);
}

void MainWindow::chargerClientSelectionneAdmin()
{
    QModelIndex idx = m_tableClientsAdmin->currentIndex();
    if (!idx.isValid())
        return;

    int r = idx.row();
    m_clientNomEdit->setText(m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(r, 1)).toString());
    m_clientPrenomEdit->setText(m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(r, 2)).toString());
    m_clientEmailEdit->setText(m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(r, 3)).toString());
    m_chkClientBloque->setChecked(
        m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(r, 4)).toString() == QStringLiteral("Bloqué"));
    m_clientNouveauMdp->clear();
}

void MainWindow::modifierClientDepuisAdmin()
{
    QModelIndex idx = m_tableClientsAdmin->currentIndex();
    if (!idx.isValid())
        return;

    int idClient = m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(idx.row(), 0)).toInt();
    QString nouveauMdp = m_clientNouveauMdp->text().trimmed();
    QString erreur;

    if (!m_db->modifierClientParAdmin(m_role, idClient, m_clientNomEdit->text(),
                                      m_clientPrenomEdit->text(), m_clientEmailEdit->text(),
                                      m_chkClientBloque->isChecked(), nouveauMdp, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Modification refusée"), erreur);
        return;
    }

    if (!nouveauMdp.isEmpty()) {
        ConfigSmtp cfg = SmtpClient::chargerConfiguration();
        if (cfg.estConfigure()) {
            QString errSmtp;
            SmtpClient::envoyerEmailTexteSimple(
                cfg, m_clientEmailEdit->text().trimmed(),
                QStringLiteral("Réinitialisation de votre mot de passe"),
                QStringLiteral("Bonjour,\n\nVotre nouveau mot de passe provisoire est : %1").arg(nouveauMdp),
                errSmtp);
        }
    }

    m_clientNouveauMdp->clear();
    actualiserListeClientsAdmin();
    QMessageBox::information(this, QStringLiteral("Client mis à jour"),
                             QStringLiteral("Compte client mis à jour et notifié dans sa Boîte de réception."));
}

void MainWindow::supprimerClientDepuisAdmin()
{
    QModelIndex idx = m_tableClientsAdmin->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, QStringLiteral("Sélection requise"),
                                 QStringLiteral("Sélectionnez un client dans la liste."));
        return;
    }

    const int idClient = m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(idx.row(), 0)).toInt();
    const QString nom = m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(idx.row(), 1)).toString();
    const QString prenom = m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(idx.row(), 2)).toString();
    const QString email = m_modeleClientsAdmin->data(m_modeleClientsAdmin->index(idx.row(), 3)).toString();

    if (QMessageBox::question(
            this, QStringLiteral("Confirmer la suppression"),
            QStringLiteral("Supprimer définitivement le client %1 %2 (%3) ainsi que ses réservations ?")
                .arg(prenom, nom, email)) != QMessageBox::Yes) {
        return;
    }

    QString erreur;
    if (!m_db->supprimerClientParAdmin(m_role, idClient, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Suppression refusée"), erreur);
        return;
    }

    m_clientNomEdit->clear();
    m_clientPrenomEdit->clear();
    m_clientEmailEdit->clear();
    m_clientNouveauMdp->clear();
    m_chkClientBloque->setChecked(false);

    actualiserEspaceAdmin();
    executerRechercheOuAffichageComplet(false);
    QMessageBox::information(this, QStringLiteral("Client supprimé"),
                             QStringLiteral("Le compte client a été supprimé avec succès."));
}

void MainWindow::sauvegarderConfigSmtpAdmin()
{
    ConfigSmtp cfg;
    cfg.serveur = m_smtpServeur->text().trimmed();
    cfg.port = m_smtpPort->value();
    cfg.identifiantSmtp = m_smtpIdentifiant->text().trimmed();
    cfg.emailExpediteur = m_smtpEmail->text().trimmed();
    cfg.motDePasseApplication = m_smtpMdpApp->text().trimmed();
    cfg.nomExpediteur = m_smtpNom->text().trimmed();
    SmtpClient::sauvegarderConfiguration(cfg);

    QSettings settings(DatabaseManager::cheminFichierConfig(), QSettings::IniFormat);
    if (m_paiementMobileMoneyCfg)
        settings.setValue(QStringLiteral("paiement/mobile_money"), m_paiementMobileMoneyCfg->text().trimmed());
    if (m_paiementCarteBancaireCfg)
        settings.setValue(QStringLiteral("paiement/carte_bancaire"), m_paiementCarteBancaireCfg->text().trimmed());
    settings.sync();

    actualiserInstructionsPaiement();
    QMessageBox::information(
        this, QStringLiteral("Configuration enregistrée"),
        QStringLiteral("Les coordonnées de paiement TicketFly et la configuration SMTP ont été sauvegardées dans config.ini."));
}

void MainWindow::chargerAdminSelectionneSuperAdmin()
{
    QModelIndex idx = m_tableAdmins->currentIndex();
    if (!idx.isValid())
        return;

    int r = idx.row();
    m_adminNom->setText(m_modeleAdmins->data(m_modeleAdmins->index(r, 1)).toString());
    m_adminPrenom->setText(m_modeleAdmins->data(m_modeleAdmins->index(r, 2)).toString());
    m_adminEmail->setText(m_modeleAdmins->data(m_modeleAdmins->index(r, 3)).toString());
    m_adminMdp->clear();
}

void MainWindow::creerCompteAdminParSuperAdmin()
{
    QString erreur;
    if (!m_db->creerAdministrateur(m_role, m_adminNom->text(), m_adminPrenom->text(),
                                   m_adminEmail->text(), m_adminMdp->text(), erreur)) {
        QMessageBox::warning(this, QStringLiteral("Création refusée"), erreur);
        return;
    }

    m_adminNom->clear();
    m_adminPrenom->clear();
    m_adminEmail->clear();
    m_adminMdp->clear();
    actualiserEspaceAdmin();
}

void MainWindow::modifierCompteAdminParSuperAdmin()
{
    QModelIndex idx = m_tableAdmins->currentIndex();
    if (!idx.isValid())
        return;

    int idCible = m_modeleAdmins->data(m_modeleAdmins->index(idx.row(), 0)).toInt();
    QString erreur;
    if (!m_db->modifierAdministrateur(m_role, idCible, m_adminNom->text(),
                                      m_adminPrenom->text(), m_adminEmail->text(),
                                      m_adminMdp->text(), erreur)) {
        QMessageBox::warning(this, QStringLiteral("Modification refusée"), erreur);
        return;
    }

    m_adminMdp->clear();
    actualiserEspaceAdmin();
}

void MainWindow::supprimerCompteAdminParSuperAdmin()
{
    QModelIndex idx = m_tableAdmins->currentIndex();
    if (!idx.isValid())
        return;

    int idCible = m_modeleAdmins->data(m_modeleAdmins->index(idx.row(), 0)).toInt();
    QString roleCible = m_modeleAdmins->data(m_modeleAdmins->index(idx.row(), 4)).toString();
    if (roleCible == QStringLiteral("super_admin")) {
        QMessageBox::warning(this, QStringLiteral("Action interdite"),
                             QStringLiteral("Le compte Super Administrateur ne peut pas être supprimé."));
        return;
    }

    QString erreur;
    if (!m_db->supprimerAdministrateur(m_role, idCible, erreur)) {
        QMessageBox::warning(this, QStringLiteral("Action refusée"), erreur);
        return;
    }
    actualiserEspaceAdmin();
}

// ============================================================================
// SONDAGE PÉRIODIQUE MULTI-INSTANCES (SANS BANNIÈRE INTRUSIVE)
// ============================================================================

void MainWindow::sonderBaseDeDonnees()
{
    if (!m_db || !m_db->estConnecte() || m_idUtilisateur <= 0)
        return;

    int nbNonLus = m_db->compterMessagesNonLus(m_idUtilisateur, m_role);
    if (nbNonLus != m_dernierNbNonLusConnu) {
        m_dernierNbNonLusConnu = nbNonLus;
        mettreAJourPastilleBoiteReception();
        if (ui->pilePages->currentIndex() == PageBoiteReception)
            actualiserBoiteReception();
    }

    if (m_role == QStringLiteral("admin") || m_role == QStringLiteral("super_admin")) {
        StatistiquesAdmin stats = m_db->obtenirStatistiquesAdmin();
        if (m_dernierIdAttenteConnu >= 0 && stats.dernierIdEnAttente > m_dernierIdAttenteConnu) {
            m_dernierIdAttenteConnu = stats.dernierIdEnAttente;
            actualiserEspaceAdmin();
        } else if (m_dernierIdAttenteConnu < 0) {
            m_dernierIdAttenteConnu = stats.dernierIdEnAttente;
        }
    } else if (m_role == QStringLiteral("client")) {
        QString empreinteActuelle = m_db->empreinteReservationsClient(m_idUtilisateur);
        if (!m_derniereEmpreinteClient.isEmpty() && empreinteActuelle != m_derniereEmpreinteClient) {
            m_derniereEmpreinteClient = empreinteActuelle;
            mettreAJourPastilleBoiteReception();
            if (ui->pilePages->currentIndex() == PageMesReservations)
                actualiserMesReservations();
        }
    }
}
