#include "escaledialog.h"
#include "databasemanager.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QAbstractButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

// Seuils par défaut (à rendre configurables plus tard)
static const int MARGE_CONFORT_MIN = 45;     // marge >= 45 min : confortable
static const int ESCALE_MIN_SEPARES = 180;   // billets séparés : 3 h minimum
static const int ESCALE_MIN_CHGT_AERO = 240; // changement d'aéroport : 4 h minimum

// Données mémorisées dans la 1re cellule de chaque ligne
enum {
    R_ID1 = Qt::UserRole, R_ID2, R_TYPE, R_MARGE, R_NUM1, R_NUM2, R_TRAJET,
    R_VISA, R_CHGT_AERO
};

static QString formatDuree(qint64 minutes)
{
    return QString::number(minutes / 60) + " h "
           + QString("%1").arg(minutes % 60, 2, 10, QChar('0'));
}

// Fenêtre d'avertissement avec case à cocher obligatoire
static bool confirmerAvecCase(QWidget *parent, const QString &titre,
                              const QString &texte, const QString &libelleCase)
{
    QMessageBox msg(QMessageBox::Warning, titre, texte,
                    QMessageBox::Ok | QMessageBox::Cancel, parent);
    QCheckBox *cb = new QCheckBox(libelleCase);
    msg.setCheckBox(cb);
    QAbstractButton *okBtn = msg.button(QMessageBox::Ok);
    okBtn->setEnabled(false);
    QObject::connect(cb, &QCheckBox::toggled, okBtn, &QAbstractButton::setEnabled);
    return msg.exec() == QMessageBox::Ok;
}

EscaleDialog::EscaleDialog(DatabaseManager *db, int idUtilisateur, QWidget *parent)
    : QDialog(parent), m_db(db), m_idUtilisateur(idUtilisateur)
{
    setWindowTitle("Vols avec une escale");
    resize(1250, 460);

    m_depart = new QLineEdit;
    m_depart->setPlaceholderText("Ville de départ");
    m_arrivee = new QLineEdit;
    m_arrivee->setPlaceholderText("Ville d'arrivée");
    m_nat = new QComboBox;
    m_nat->addItems({"Française", "Indienne", "Marocaine", "Nigériane", "Autre"});
    m_nb = new QSpinBox;
    m_nb->setRange(1, 99);
    m_nb->setPrefix("Passagers : ");
    m_risque = new QCheckBox("Afficher les correspondances risquées");
    QPushButton *btn = new QPushButton("Rechercher");
    btn->setDefault(true);

    QHBoxLayout *haut = new QHBoxLayout;
    haut->addWidget(m_depart);
    haut->addWidget(m_arrivee);
    haut->addWidget(new QLabel("Nationalité :"));
    haut->addWidget(m_nat);
    haut->addWidget(m_nb);
    haut->addWidget(btn);

    m_info = new QLabel;
    QLabel *aide = new QLabel("Double-cliquez sur un itinéraire pour le réserver. "
                              "Les règles de visa affichées sont des données de démonstration.");

    m_table = new QTableWidget(0, 10);
    m_table->setHorizontalHeaderLabels({"Trajet", "Vol 1", "Vol 2", "Départ",
                                        "Arrivée", "Escale", "Correspondance",
                                        "Billet", "Visa de transit", "Prix total"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setStretchLastSection(true);

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->addLayout(haut);
    principal->addWidget(m_risque);
    principal->addWidget(m_info);
    principal->addWidget(aide);
    principal->addWidget(m_table);

    connect(btn, &QPushButton::clicked, this, &EscaleDialog::rechercher);
    connect(m_depart, &QLineEdit::returnPressed, this, &EscaleDialog::rechercher);
    connect(m_arrivee, &QLineEdit::returnPressed, this, &EscaleDialog::rechercher);
    connect(m_risque, &QCheckBox::toggled, this, &EscaleDialog::rechercher);
    connect(m_nat, &QComboBox::currentIndexChanged, this,
            [this]() { rechercher(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { reserver(row); });
}

void EscaleDialog::rechercher()
{
    m_table->setRowCount(0);

    QSqlQuery q(m_db->database());
    q.prepare(
        "SELECT v1.numero_vol, v2.numero_vol, v1.ville_depart, v1.ville_arrivee, "
        "       v2.ville_arrivee, v1.date_depart, v2.date_arrivee, "
        "       v1.prix + v2.prix, "
        "       TIMESTAMPDIFF(MINUTE, v1.date_arrivee, v2.date_depart), "
        "       IFNULL(a.mct_minutes, 45), "
        "       v1.id_vol, v2.id_vol, v1.compagnie, v2.compagnie, "
        "       IFNULL(v1.aeroport_arrivee, ''), IFNULL(v2.aeroport_depart, ''), "
        "       IFNULL(r.statut, 'non') "
        "FROM VOL v1 "
        "JOIN VOL v2 ON v2.ville_depart = v1.ville_arrivee "
        "           AND v2.ville_arrivee <> v1.ville_depart "
        "LEFT JOIN AEROPORT a ON a.code_iata = v1.aeroport_arrivee "
        "LEFT JOIN PAYS_REGLE_TRANSIT r ON r.nationalite = :nat AND r.pays = a.pays "
        "WHERE v1.ville_depart LIKE :d AND v2.ville_arrivee LIKE :a "
        "  AND v1.date_depart > NOW() "
        "  AND v1.statut_vol <> 'annule' AND v2.statut_vol <> 'annule' "
        "  AND v2.date_depart > v1.date_arrivee "
        "  AND TIMESTAMPDIFF(MINUTE, v1.date_arrivee, v2.date_depart) <= 1440 "
        "  AND v1.places_disponibles >= :nb AND v2.places_disponibles >= :nb "
        "ORDER BY v1.date_depart, v2.date_depart");
    q.bindValue(":nat", m_nat->currentText());
    q.bindValue(":d", "%" + m_depart->text().trimmed() + "%");
    q.bindValue(":a", "%" + m_arrivee->text().trimmed() + "%");
    q.bindValue(":nb", m_nb->value());

    if (!q.exec()) {
        qDebug() << "Erreur recherche escales :" << q.lastError().text();
        m_info->setText("Erreur de recherche (voir l'onglet 3 Application).");
        return;
    }

    int affichees = 0, masquees = 0;
    while (q.next()) {
        qint64 minutes = q.value(8).toLongLong();
        int mct = q.value(9).toInt();
        QString aero1 = q.value(14).toString();
        QString aero2 = q.value(15).toString();
        QString visa = q.value(16).toString();

        // Changement d'aéroport dans la même ville
        bool chgtAero = !aero1.isEmpty() && !aero2.isEmpty() && aero1 != aero2;
        // Même compagnie sans changement d'aéroport : billet unique (protégé)
        bool unique = !chgtAero
                      && q.value(12).toString().compare(
                             q.value(13).toString(), Qt::CaseInsensitive) == 0;
        QString type = unique ? "unique" : "separe";

        int requis = mct;
        if (chgtAero)
            requis = qMax(mct, ESCALE_MIN_CHGT_AERO);
        else if (!unique)
            requis = qMax(mct, ESCALE_MIN_SEPARES);
        qint64 marge = minutes - requis;

        QString niveau;
        QColor couleur;
        if (marge < 0) {
            niveau = "Risquée (trop courte)";
            couleur = QColor(255, 199, 206);
        } else if (marge < MARGE_CONFORT_MIN) {
            niveau = "Juste";
            couleur = QColor(255, 235, 156);
        } else {
            niveau = "Confortable";
            couleur = QColor(198, 239, 206);
        }

        if (marge < 0 && !m_risque->isChecked()) {
            ++masquees;
            continue;
        }

        QString texteBillet = unique ? "Unique (protégé)" : "Séparés (sans protection)";
        if (chgtAero)
            texteBillet = "Séparés - changement d'aéroport (" + aero1 + " → " + aero2 + ")";

        QString texteVisa = "Aucun visa de transit requis";
        if (visa == "verifier")
            texteVisa = "Vérifier le visa de transit";
        else if (visa == "probable")
            texteVisa = "Visa de transit probablement requis";

        QString trajet = q.value(2).toString() + " → " + q.value(3).toString()
                         + " → " + q.value(4).toString();
        QStringList cellules = {
            trajet,
            q.value(0).toString(),
            q.value(1).toString(),
            q.value(5).toDateTime().toString("dd/MM HH:mm"),
            q.value(6).toDateTime().toString("dd/MM HH:mm"),
            formatDuree(minutes) + " (min. " + QString::number(requis) + " min)",
            niveau,
            texteBillet,
            texteVisa,
            QString::number(q.value(7).toDouble(), 'f', 2) + " EUR"
        };

        int row = m_table->rowCount();
        m_table->insertRow(row);
        for (int c = 0; c < cellules.size(); ++c) {
            QTableWidgetItem *it = new QTableWidgetItem(cellules[c]);
            it->setBackground(couleur);
            it->setForeground(Qt::black);
            m_table->setItem(row, c, it);
        }
        QTableWidgetItem *it0 = m_table->item(row, 0);
        it0->setData(R_ID1, q.value(10).toInt());
        it0->setData(R_ID2, q.value(11).toInt());
        it0->setData(R_TYPE, type);
        it0->setData(R_MARGE, marge);
        it0->setData(R_NUM1, q.value(0).toString());
        it0->setData(R_NUM2, q.value(1).toString());
        it0->setData(R_TRAJET, trajet);
        it0->setData(R_VISA, visa);
        it0->setData(R_CHGT_AERO, chgtAero ? aero1 + " → " + aero2 : QString());
        ++affichees;
    }
    m_table->resizeColumnsToContents();

    QString info = QString::number(affichees) + " itinéraire(s) avec escale.";
    if (masquees > 0)
        info += "  " + QString::number(masquees)
                + " correspondance(s) trop courte(s) masquée(s).";
    m_info->setText(info);
}

void EscaleDialog::reserver(int row)
{
    // 1. Un visiteur doit d'abord avoir un compte
    if (m_idUtilisateur <= 0) {
        QMessageBox::information(this, "Connexion requise",
                                 "Vous devez avoir un compte pour réserver.\n\n"
                                 "Fermez cette fenêtre, puis utilisez « Se déconnecter » dans la barre "
                                 "d'outils de la fenêtre principale pour ouvrir la connexion / inscription.");
        return;
    }

    QTableWidgetItem *it0 = m_table->item(row, 0);
    if (!it0)
        return;
    int id1 = it0->data(R_ID1).toInt();
    int id2 = it0->data(R_ID2).toInt();
    QString type = it0->data(R_TYPE).toString();
    qint64 marge = it0->data(R_MARGE).toLongLong();
    QString trajet = it0->data(R_TRAJET).toString();
    QString visa = it0->data(R_VISA).toString();
    QString chgtAero = it0->data(R_CHGT_AERO).toString();

    // 2. Une correspondance trop courte n'est pas vendable
    if (marge < 0) {
        QMessageBox::warning(this, "Réservation impossible",
                             "La correspondance est trop courte : cet itinéraire ne peut pas être réservé.");
        return;
    }

    // 3. Changement d'aéroport : avertissement obligatoire
    if (!chgtAero.isEmpty()) {
        if (!confirmerAvecCase(this, "Changement d'aéroport",
                               "Vous arrivez et repartez de deux aéroports différents ("
                                   + chgtAero + ").\n\n"
                                                "• Vous devrez effectuer le trajet entre les aéroports par vos propres moyens.\n"
                                                "• Vous devrez peut-être passer la frontière et refaire les formalités.\n"
                                                "• Les deux vols sont traités comme des billets séparés, sans protection.",
                               "J'ai compris et j'accepte ces conditions"))
            return;
    } else if (type == "separe") {
        // 4. Billets séparés : avertissement obligatoire
        if (!confirmerAvecCase(this, "Billets séparés - attention",
                               "Ces deux vols sont deux billets indépendants (compagnies différentes).\n\n"
                               "• Si le premier vol est en retard, vous perdez le second SANS compensation.\n"
                               "• Aucun réacheminement ni remboursement n'est garanti.\n"
                               "• Vous devrez récupérer vos bagages et les enregistrer de nouveau.",
                               "J'ai compris et j'accepte ces risques"))
            return;
    }

    // 5. Conditions d'entrée et de transit (case obligatoire avant réservation)
    QString texteVisa = "Aucun visa de transit requis selon nos données.";
    if (visa == "verifier")
        texteVisa = "Un visa de transit peut être nécessaire selon votre situation.";
    else if (visa == "probable")
        texteVisa = "Un visa de transit est probablement nécessaire pour cette escale.";
    if (!confirmerAvecCase(this, "Conditions d'entrée et de transit",
                           texteVisa + "\n\nCes informations sont indicatives (données de démonstration). "
                                       "Seules les autorités officielles font foi.",
                           "J'ai vérifié les conditions d'entrée et de transit auprès des autorités officielles"))
        return;

    // 6. Nom du passager
    bool ok = false;
    QString passager = QInputDialog::getText(this, "Réservation",
                                             "Nom du passager :", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || passager.isEmpty()) {
        QMessageBox::warning(this, "Réservation", "Le nom du passager est obligatoire.");
        return;
    }

    // 7. Enregistrement (les deux vols, ou aucun)
    QString refGroupe;
    if (m_db->enregistrerItineraire(id1, id2, passager, m_nb->value(), type,
                                    m_idUtilisateur, refGroupe)) {
        QMessageBox::information(this, "Réservation confirmée",
                                 "Itinéraire : " + trajet + "\n"
                                                            "Vols : " + it0->data(R_NUM1).toString() + " + "
                                     + it0->data(R_NUM2).toString() + "\n"
                                                                      "Passager : " + passager + "\n"
                                                  "Billet : " + (type == "unique" ? "unique (protégé)" : "séparés") + "\n"
                                                                                             "Référence : " + refGroupe);
        rechercher();
    } else {
        QMessageBox::critical(this, "Réservation",
                              "Impossible d'enregistrer : plus assez de places, vol déjà parti "
                              "ou erreur de base.");
    }
}