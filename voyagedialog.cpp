#include "voyagedialog.h"
#include "databasemanager.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

enum { R_IDVOL = Qt::UserRole, R_STATUT_VOL };

VoyageDialog::VoyageDialog(DatabaseManager *db, int idUtilisateur, QWidget *parent)
    : QDialog(parent), m_db(db), m_id(idUtilisateur)
{
    setWindowTitle("Mon voyage");
    resize(1050, 600);

    m_res = new QTableWidget(0, 8);
    m_res->setHorizontalHeaderLabels({"Réservation", "Vol", "Trajet", "Départ prévu",
                                      "Arrivée prévue", "État du vol",
                                      "Statut réservation", "Itinéraire"});
    m_res->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_res->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_res->setSelectionMode(QAbstractItemView::SingleSelection);
    m_res->horizontalHeader()->setStretchLastSection(true);

    m_alertes = new QTableWidget(0, 4);
    m_alertes->setHorizontalHeaderLabels({"Date", "Type", "Message", "État"});
    m_alertes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alertes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alertes->horizontalHeader()->setStretchLastSection(true);

    QPushButton *btnAlt = new QPushButton("Alternatives pour le vol sélectionné");
    QPushButton *btnDroits = new QPushButton("Mes droits");
    QPushButton *btnLire = new QPushButton("Marquer les alertes comme lues");
    QPushButton *btnActu = new QPushButton("Actualiser");
    QHBoxLayout *boutons = new QHBoxLayout;
    boutons->addWidget(btnAlt);
    boutons->addWidget(btnDroits);
    boutons->addWidget(btnLire);
    boutons->addWidget(btnActu);

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->addWidget(new QLabel("<b>Mes réservations</b>"));
    principal->addWidget(m_res, 3);
    principal->addLayout(boutons);
    principal->addWidget(new QLabel("<b>Mes alertes</b>"));
    principal->addWidget(m_alertes, 2);

    connect(btnAlt, &QPushButton::clicked, this, &VoyageDialog::alternatives);
    connect(btnDroits, &QPushButton::clicked, this, &VoyageDialog::droits);
    connect(btnLire, &QPushButton::clicked, this, &VoyageDialog::toutLire);
    connect(btnActu, &QPushButton::clicked, this, &VoyageDialog::charger);

    charger();
}

void VoyageDialog::charger()
{
    // ----- Réservations -----
    m_res->setRowCount(0);
    QSqlQuery q(m_db->database());
    q.prepare("SELECT r.reference, v.numero_vol, v.ville_depart, v.ville_arrivee, "
              "v.date_depart, v.date_arrivee, v.statut_vol, v.retard_minutes, "
              "r.statut, IFNULL(r.ref_groupe, ''), r.id_vol "
              "FROM RESERVATION r JOIN VOL v ON v.id_vol = r.id_vol "
              "WHERE r.id_utilisateur = :u ORDER BY v.date_depart");
    q.bindValue(":u", m_id);
    if (!q.exec())
        qDebug() << "Erreur Mon voyage :" << q.lastError().text();

    while (q.next()) {
        int retard = q.value(7).toInt();
        QString statutVol = q.value(6).toString();
        QDateTime dep = q.value(4).toDateTime().addSecs(qint64(retard) * 60);
        QDateTime arr = q.value(5).toDateTime().addSecs(qint64(retard) * 60);

        QString etat = "À l'heure";
        QColor couleur(198, 239, 206);
        if (statutVol == "annule") {
            etat = "ANNULÉ";
            couleur = QColor(255, 199, 206);
        } else if (statutVol == "retarde") {
            etat = "Retardé de " + QString::number(retard) + " min";
            couleur = QColor(255, 235, 156);
        }

        QStringList cellules = {
            q.value(0).toString(),
            q.value(1).toString(),
            q.value(2).toString() + " → " + q.value(3).toString(),
            dep.toString("dd/MM HH:mm"),
            arr.toString("dd/MM HH:mm"),
            etat,
            q.value(8).toString(),
            q.value(9).toString()
        };
        int row = m_res->rowCount();
        m_res->insertRow(row);
        for (int c = 0; c < cellules.size(); ++c) {
            QTableWidgetItem *it = new QTableWidgetItem(cellules[c]);
            if (c == 5) {
                it->setBackground(couleur);
                it->setForeground(Qt::black);
            }
            m_res->setItem(row, c, it);
        }
        m_res->item(row, 0)->setData(R_IDVOL, q.value(10).toInt());
        m_res->item(row, 0)->setData(R_STATUT_VOL, statutVol);
    }
    m_res->resizeColumnsToContents();

    // ----- Alertes -----
    m_alertes->setRowCount(0);
    QSqlQuery a(m_db->database());
    a.prepare("SELECT date_creation, type, message, lue FROM ALERTE "
              "WHERE id_utilisateur = :u ORDER BY id_alerte DESC");
    a.bindValue(":u", m_id);
    if (!a.exec())
        qDebug() << "Erreur alertes :" << a.lastError().text();

    while (a.next()) {
        QString type = a.value(1).toString();
        if (type == "retard") type = "Retard";
        else if (type == "annulation") type = "Annulation";
        else if (type == "correspondance") type = "Correspondance";
        bool lue = a.value(3).toInt() != 0;

        QStringList cellules = {
            a.value(0).toDateTime().toString("dd/MM HH:mm"),
            type,
            a.value(2).toString(),
            lue ? "Lue" : "Nouvelle"
        };
        int row = m_alertes->rowCount();
        m_alertes->insertRow(row);
        for (int c = 0; c < cellules.size(); ++c) {
            QTableWidgetItem *it = new QTableWidgetItem(cellules[c]);
            if (!lue) {
                QFont f = it->font();
                f.setBold(true);
                it->setFont(f);
            }
            m_alertes->setItem(row, c, it);
        }
    }
    m_alertes->resizeColumnsToContents();
}

void VoyageDialog::toutLire()
{
    QSqlQuery q(m_db->database());
    q.prepare("UPDATE ALERTE SET lue = 1 WHERE id_utilisateur = :u");
    q.bindValue(":u", m_id);
    q.exec();
    charger();
}

void VoyageDialog::alternatives()
{
    int row = m_res->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Alternatives",
                                 "Sélectionnez d'abord une réservation dans la liste.");
        return;
    }
    int idVol = m_res->item(row, 0)->data(R_IDVOL).toInt();

    QSqlQuery q(m_db->database());
    q.prepare("SELECT ville_depart, ville_arrivee FROM VOL WHERE id_vol = :id");
    q.bindValue(":id", idVol);
    if (!q.exec() || !q.next())
        return;
    QString dep = q.value(0).toString();
    QString arr = q.value(1).toString();
    q.finish();

    q.prepare("SELECT numero_vol, date_depart, date_arrivee, prix, places_disponibles "
              "FROM VOL WHERE ville_depart = :d AND ville_arrivee = :a "
              "AND id_vol <> :id AND statut_vol <> 'annule' AND date_depart > NOW() "
              "ORDER BY date_arrivee LIMIT 10");
    q.bindValue(":d", dep);
    q.bindValue(":a", arr);
    q.bindValue(":id", idVol);
    if (!q.exec()) {
        qDebug() << "Erreur alternatives :" << q.lastError().text();
        return;
    }

    QString texte;
    while (q.next()) {
        texte += q.value(0).toString() + "  |  départ "
                 + q.value(1).toDateTime().toString("dd/MM HH:mm")
                 + "  |  arrivée " + q.value(2).toDateTime().toString("dd/MM HH:mm")
                 + "  |  " + QString::number(q.value(3).toDouble(), 'f', 2) + " EUR"
                 + "  |  " + q.value(4).toString() + " place(s)\n";
    }
    if (texte.isEmpty())
        texte = "Aucun vol alternatif disponible sur ce trajet pour le moment.";
    else
        texte += "\nPour réserver un de ces vols, utilisez la recherche de la fenêtre principale.";

    QMessageBox::information(this, "Alternatives " + dep + " → " + arr, texte);
}

void VoyageDialog::droits()
{
    QMessageBox::information(this, "Mes droits (informations indicatives)",
                             "En cas d'annulation ou de gros retard, la compagnie doit en principe vous "
                             "proposer un réacheminement ou un remboursement, et peut devoir prendre en "
                             "charge repas, hébergement et transport selon la situation.\n\n"
                             "Une indemnisation peut aussi s'appliquer selon la réglementation en vigueur "
                             "(par exemple en Europe) et la cause de la perturbation.\n\n"
                             "Attention : avec des billets séparés, ces protections ne s'appliquent pas "
                             "d'un billet à l'autre.\n\n"
                             "Ces informations sont indicatives : seule la réglementation officielle "
                             "et les conditions de votre compagnie font foi.");
}