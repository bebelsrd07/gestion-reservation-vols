#include "seatmapdialog.h"
#include "databasemanager.h"
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static QLabel *legende(const QString &style, const QString &texte)
{
    QLabel *l = new QLabel(texte);
    l->setStyleSheet(style + " border-radius:3px; padding:2px 6px; color:#000;");
    return l;
}

SeatMapDialog::SeatMapDialog(DatabaseManager *db, int idVol, int nb,
                             const QString &titre, QWidget *parent)
    : QDialog(parent), m_db(db), m_idVol(idVol), m_nb(nb)
{
    setWindowTitle("Choix des sièges");
    resize(540, 700);
    setStyleSheet(
        "QPushButton#siege { background:#c6efce; color:#000; border:1px solid #6aa84f;"
        "                    border-radius:4px; }"
        "QPushButton#siege[payant=\"true\"] { border:2px solid #e69138; }"
        "QPushButton#siege:checked { background:#4a90e2; color:#fff; border:1px solid #1c5fb0; }"
        "QPushButton#siege:disabled { background:#bdbdbd; color:#777; border:1px solid #999; }");

    QLabel *entete = new QLabel("<b>" + titre.toHtmlEscaped() + "</b><br>Choisissez "
                                + QString::number(nb) + " siège(s).");
    entete->setWordWrap(true);

    QHBoxLayout *leg = new QHBoxLayout;
    leg->addWidget(legende("background:#c6efce; border:1px solid #6aa84f;", "Libre"));
    leg->addWidget(legende("background:#4a90e2; border:1px solid #1c5fb0; color:#fff;", "Choisi"));
    leg->addWidget(legende("background:#bdbdbd; border:1px solid #999;", "Occupé"));
    leg->addWidget(legende("background:#c6efce; border:2px solid #e69138;", "Payant"));
    leg->addStretch();

    // ----- Plan de la cabine : A B C | allée | D E F -----
    QWidget *carte = new QWidget;
    QGridLayout *grille = new QGridLayout(carte);
    grille->setSpacing(4);
    grille->setContentsMargins(12, 8, 12, 8);
    grille->setColumnMinimumWidth(4, 30);

    static const char lettres[] = "ABCDEF";
    static const int colonnes[6] = { 1, 2, 3, 5, 6, 7 };
    for (int i = 0; i < 6; ++i) {
        QLabel *l = new QLabel(QString(QChar::fromLatin1(lettres[i])));
        l->setAlignment(Qt::AlignCenter);
        grille->addWidget(l, 0, colonnes[i]);
    }

    QSqlQuery q(m_db->database());
    q.prepare("SELECT id_siege, numero, rangee, lettre, categorie, supplement, "
              "id_reservation IS NOT NULL FROM SIEGE WHERE id_vol = :v "
              "ORDER BY rangee, lettre");
    q.bindValue(":v", m_idVol);
    if (!q.exec())
        qDebug() << "Erreur plan de cabine :" << q.lastError().text();

    int derniereRangee = -1, ligne = 0;
    while (q.next()) {
        int id = q.value(0).toInt();
        QString numero = q.value(1).toString();
        int rangee = q.value(2).toInt();
        QString lettre = q.value(3).toString();
        QString cat = q.value(4).toString();
        double supp = q.value(5).toDouble();
        bool pris = q.value(6).toInt() != 0;

        int idx = QString("ABCDEF").indexOf(lettre);
        if (idx < 0)
            continue;

        if (rangee != derniereRangee) {             // nouvelle rangée
            ++ligne;
            derniereRangee = rangee;
            for (int col : { 0, 4 }) {
                QLabel *n = new QLabel(QString::number(rangee));
                n->setAlignment(Qt::AlignCenter);
                n->setStyleSheet("color:#888;");
                grille->addWidget(n, ligne, col);
            }
        }

        QPushButton *b = new QPushButton(numero);
        b->setObjectName("siege");
        b->setCheckable(true);
        b->setFixedSize(42, 32);
        b->setProperty("payant", supp > 0);

        QString nomCat = (cat == "avant") ? "avant de la cabine"
                         : (cat == "sortie") ? "sortie de secours" : "standard";
        if (pris) {
            b->setEnabled(false);
            b->setToolTip(numero + " : occupé");
        } else {
            b->setToolTip(numero + " : " + nomCat
                          + (supp > 0 ? " (+" + QString::number(supp, 'f', 2) + " EUR)"
                                      : " (gratuit)"));
        }
        grille->addWidget(b, ligne, colonnes[idx]);

        m_boutons.insert(id, b);
        m_supp.insert(id, supp);
        m_num.insert(id, numero);

        connect(b, &QPushButton::clicked, this, [this, b](bool coche) {
            if (coche && m_nb == 1) {               // 1 passager : le clic déplace le choix
                for (auto it = m_boutons.constBegin(); it != m_boutons.constEnd(); ++it)
                    if (it.value() != b)
                        it.value()->setChecked(false);
            } else if (coche && nbChoisis() > m_nb) {
                b->setChecked(false);
                m_info->setText("Vous avez déjà choisi " + QString::number(m_nb)
                                + " siège(s). Décochez-en un d'abord.");
                return;
            }
            majInfos();
        });
    }

    QScrollArea *zone = new QScrollArea;
    zone->setWidget(carte);
    zone->setWidgetResizable(true);

    m_info = new QLabel;
    m_info->setWordWrap(true);

    QPushButton *btnAuto = new QPushButton("Placement automatique");
    QPushButton *btnEff = new QPushButton("Effacer");
    m_valider = new QPushButton("Valider mes sièges");
    QPushButton *btnAnn = new QPushButton("Annuler");
    m_valider->setEnabled(false);
    QHBoxLayout *boutons = new QHBoxLayout;
    boutons->addWidget(btnAuto);
    boutons->addWidget(btnEff);
    boutons->addStretch();
    boutons->addWidget(m_valider);
    boutons->addWidget(btnAnn);

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->addWidget(entete);
    principal->addLayout(leg);
    principal->addWidget(new QLabel("▲ Avant de l'appareil"));
    principal->addWidget(zone, 1);
    principal->addWidget(new QLabel("▼ Arrière de l'appareil"));
    principal->addWidget(m_info);
    principal->addLayout(boutons);

    connect(btnAuto, &QPushButton::clicked, this, &SeatMapDialog::placementAuto);
    connect(btnEff, &QPushButton::clicked, this, &SeatMapDialog::effacer);
    connect(m_valider, &QPushButton::clicked, this, &QDialog::accept);
    connect(btnAnn, &QPushButton::clicked, this, &QDialog::reject);

    if (m_boutons.isEmpty())
        m_info->setText("Aucun siège défini pour ce vol.");
    else
        majInfos();
}

int SeatMapDialog::nbChoisis() const
{
    int n = 0;
    for (auto it = m_boutons.constBegin(); it != m_boutons.constEnd(); ++it)
        if (it.value()->isChecked())
            ++n;
    return n;
}

void SeatMapDialog::majInfos()
{
    QStringList nums;
    double total = 0.0;
    for (auto it = m_boutons.constBegin(); it != m_boutons.constEnd(); ++it) {
        if (it.value()->isChecked()) {
            nums << m_num.value(it.key());
            total += m_supp.value(it.key());
        }
    }
    m_info->setText("Sièges choisis (" + QString::number(nums.size()) + "/"
                    + QString::number(m_nb) + ") : "
                    + (nums.isEmpty() ? QString("aucun") : nums.join(", "))
                    + "\nSupplément sièges : " + QString::number(total, 'f', 2) + " EUR");
    m_valider->setEnabled(nums.size() == m_nb);
}

void SeatMapDialog::effacer()
{
    for (auto it = m_boutons.constBegin(); it != m_boutons.constEnd(); ++it)
        it.value()->setChecked(false);
    majInfos();
}

void SeatMapDialog::placementAuto()
{
    const QList<int> ids = m_db->siegesAutomatiques(m_idVol, m_nb);
    bool ok = !ids.isEmpty();
    for (int id : ids)
        if (!m_boutons.contains(id))
            ok = false;
    if (!ok) {
        QMessageBox::warning(this, "Placement automatique",
                             "Impossible de placer automatiquement les passagers : "
                             "pas assez de sièges libres.");
        return;
    }
    effacer();
    for (int id : ids)
        m_boutons.value(id)->setChecked(true);
    majInfos();
}

QList<int> SeatMapDialog::sieges() const
{
    QList<int> res;
    for (auto it = m_boutons.constBegin(); it != m_boutons.constEnd(); ++it)
        if (it.value()->isChecked())
            res.append(it.key());
    return res;
}