#include "admindialog.h"
#include "databasemanager.h"
#include <QTabWidget>
#include <QTableView>
#include <QSqlQueryModel>
#include <QSqlQuery>
#include <QSqlError>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include <QInputDialog>

AdminDialog::AdminDialog(DatabaseManager *db, QWidget *parent)
    : QDialog(parent), m_db(db), m_modelVols(new QSqlQueryModel(this)),
    m_modelRes(new QSqlQueryModel(this))
{
    setWindowTitle("Administration");
    resize(850, 480);

    m_onglets = new QTabWidget(this);

    // ----- Onglet Tableau de bord -----
    QWidget *pageBord = new QWidget;
    QVBoxLayout *vBord = new QVBoxLayout(pageBord);
    m_lblVols = new QLabel;
    m_lblTotal = new QLabel;
    m_lblConf = new QLabel;
    m_lblAnn = new QLabel;
    QFont f = m_lblVols->font();
    f.setPointSize(13);
    for (QLabel *l : {m_lblVols, m_lblTotal, m_lblConf, m_lblAnn})
        l->setFont(f);
    QPushButton *btnActu = new QPushButton("Actualiser");
    vBord->addWidget(m_lblVols);
    vBord->addWidget(m_lblTotal);
    vBord->addWidget(m_lblConf);
    vBord->addWidget(m_lblAnn);
    vBord->addWidget(btnActu);
    vBord->addStretch();

    // ----- Onglet Vols -----
    QWidget *pageVols = new QWidget;
    QVBoxLayout *vVols = new QVBoxLayout(pageVols);
    m_tableVols = new QTableView;
    m_tableVols->setModel(m_modelVols);
    m_tableVols->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableVols->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableVols->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QPushButton *btnAjouter = new QPushButton("Ajouter un vol");
    QPushButton *btnModifier = new QPushButton("Modifier le vol sélectionné");
    QPushButton *btnSupprimer = new QPushButton("Supprimer le vol sélectionné");
    QPushButton *btnSimul = new QPushButton("Simuler retard / annulation");
    QHBoxLayout *hBtn = new QHBoxLayout;
    hBtn->addWidget(btnAjouter);
    hBtn->addWidget(btnModifier);
    hBtn->addWidget(btnSupprimer);
    hBtn->addWidget(btnSimul);
    vVols->addWidget(m_tableVols);
    vVols->addLayout(hBtn);

    // ----- Onglet Réservations -----
    QWidget *pageRes = new QWidget;
    QVBoxLayout *vRes = new QVBoxLayout(pageRes);
    m_tableRes = new QTableView;
    m_tableRes->setModel(m_modelRes);
    m_tableRes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableRes->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableRes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QPushButton *btnConf = new QPushButton("Marquer comme confirmée");
    QPushButton *btnAnn = new QPushButton("Marquer comme annulée");
    QHBoxLayout *hRes = new QHBoxLayout;
    hRes->addWidget(btnConf);
    hRes->addWidget(btnAnn);
    vRes->addWidget(m_tableRes);
    vRes->addLayout(hRes);

    connect(btnConf, &QPushButton::clicked, this, [this]() { changerStatut("confirmee"); });
    connect(btnAnn, &QPushButton::clicked, this, [this]() { changerStatut("annulee"); });

    m_onglets->addTab(pageBord, "Tableau de bord");
    m_onglets->addTab(pageVols, "Vols");
    m_onglets->addTab(pageRes, "Réservations");

    QVBoxLayout *principal = new QVBoxLayout(this);
    principal->addWidget(m_onglets);

    connect(btnActu, &QPushButton::clicked, this, &AdminDialog::actualiserTout);
    connect(btnAjouter, &QPushButton::clicked, this, &AdminDialog::ajouterVol);
    connect(btnModifier, &QPushButton::clicked, this, &AdminDialog::modifierVol);
    connect(btnSupprimer, &QPushButton::clicked, this, &AdminDialog::supprimerVol);
    connect(btnSimul, &QPushButton::clicked, this, &AdminDialog::simulerPerturbation);

    actualiserTout();
}

void AdminDialog::actualiserTout()
{
    chargerStatistiques();
    chargerVols();
    chargerReservations();
}

void AdminDialog::chargerStatistiques()
{
    m_lblVols->setText("Nombre de vols enregistrés : "
                       + QString::number(m_db->nombreVols()));
    m_lblTotal->setText("Nombre total de réservations : "
                        + QString::number(m_db->nombreReservations()));
    m_lblConf->setText("Réservations confirmées : "
                       + QString::number(m_db->nombreReservations("confirmee")));
    m_lblAnn->setText("Réservations annulées : "
                      + QString::number(m_db->nombreReservations("annulee")));
}

void AdminDialog::chargerVols()
{
    QSqlQuery q(m_db->database());
    if (!q.exec("SELECT id_vol, numero_vol, compagnie, ville_depart, ville_arrivee, "
                "date_depart, date_arrivee, prix, places_disponibles, "
                "statut_vol, retard_minutes "
                "FROM VOL ORDER BY date_depart")) {
        qDebug() << "Erreur SELECT vols :" << q.lastError().text();
    }
    m_modelVols->setQuery(std::move(q));
    m_modelVols->setHeaderData(1, Qt::Horizontal, "N° vol");
    m_modelVols->setHeaderData(2, Qt::Horizontal, "Compagnie");
    m_modelVols->setHeaderData(3, Qt::Horizontal, "Départ");
    m_modelVols->setHeaderData(4, Qt::Horizontal, "Arrivée");
    m_modelVols->setHeaderData(5, Qt::Horizontal, "Date départ");
    m_modelVols->setHeaderData(6, Qt::Horizontal, "Date arrivée");
    m_modelVols->setHeaderData(7, Qt::Horizontal, "Prix (EUR)");
    m_modelVols->setHeaderData(8, Qt::Horizontal, "Places");
    m_modelVols->setHeaderData(9, Qt::Horizontal, "État");
    m_modelVols->setHeaderData(10, Qt::Horizontal, "Retard (min)");
    m_tableVols->setColumnHidden(0, true);
    m_tableVols->resizeColumnsToContents();
}

void AdminDialog::ajouterVol()
{
    formulaireVol(0);
}

void AdminDialog::modifierVol()
{
    QModelIndex idx = m_tableVols->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Modification", "Sélectionnez d'abord un vol.");
        return;
    }
    int idVol = m_modelVols->data(m_modelVols->index(idx.row(), 0)).toInt();
    formulaireVol(idVol);
}

void AdminDialog::supprimerVol()
{
    QModelIndex idx = m_tableVols->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Suppression", "Sélectionnez d'abord un vol.");
        return;
    }
    int idVol = m_modelVols->data(m_modelVols->index(idx.row(), 0)).toInt();
    QString numero = m_modelVols->data(m_modelVols->index(idx.row(), 1)).toString();

    if (QMessageBox::question(this, "Suppression",
                              "Supprimer définitivement le vol " + numero + " ?")
        != QMessageBox::Yes)
        return;

    if (m_db->supprimerVol(idVol)) {
        QMessageBox::information(this, "Suppression", "Vol supprimé.");
        actualiserTout();
    } else {
        QMessageBox::warning(this, "Suppression",
                             "Suppression impossible : ce vol a déjà des réservations.");
    }
}

void AdminDialog::formulaireVol(int idVol)
{
    QDialog dlg(this);
    dlg.setWindowTitle(idVol > 0 ? "Modifier un vol" : "Ajouter un vol");

    QLineEdit *eNum = new QLineEdit;
    QLineEdit *eComp = new QLineEdit("Air Demo");
    QLineEdit *eDep = new QLineEdit;
    QLineEdit *eArr = new QLineEdit;
    QDateTimeEdit *dDep = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7));
    QDateTimeEdit *dArr = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7).addSecs(2 * 3600));
    for (QDateTimeEdit *d : {dDep, dArr}) {
        d->setCalendarPopup(true);
        d->setDisplayFormat("dd/MM/yyyy HH:mm");
    }
    QDoubleSpinBox *sPrix = new QDoubleSpinBox;
    sPrix->setRange(0.01, 99999.99);
    sPrix->setDecimals(2);
    sPrix->setValue(100.0);
    sPrix->setSuffix(" EUR");
    QSpinBox *sPlaces = new QSpinBox;
    sPlaces->setRange(0, 600);
    sPlaces->setValue(50);

    // En modification : on remplit avec les valeurs actuelles
    if (idVol > 0) {
        QSqlQuery q(m_db->database());
        q.prepare("SELECT numero_vol, compagnie, ville_depart, ville_arrivee, "
                  "date_depart, date_arrivee, prix, capacite "
                  "FROM VOL WHERE id_vol = :id");
        q.bindValue(":id", idVol);
        if (q.exec() && q.next()) {
            eNum->setText(q.value(0).toString());
            eComp->setText(q.value(1).toString());
            eDep->setText(q.value(2).toString());
            eArr->setText(q.value(3).toString());
            dDep->setDateTime(q.value(4).toDateTime());
            dArr->setDateTime(q.value(5).toDateTime());
            sPrix->setValue(q.value(6).toDouble());
            sPlaces->setValue(q.value(7).toInt());
        }
    }

    QFormLayout *form = new QFormLayout;
    form->addRow("Numéro de vol :", eNum);
    form->addRow("Compagnie :", eComp);
    form->addRow("Ville de départ :", eDep);
    form->addRow("Ville d'arrivée :", eArr);
    form->addRow("Date de départ :", dDep);
    form->addRow("Date d'arrivée :", dArr);
    form->addRow("Prix :", sPrix);
    form->addRow("Capacité (nombre de sièges) :", sPlaces);

    QDialogButtonBox *boutons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->addLayout(form);
    lay->addWidget(boutons);
    connect(boutons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(boutons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // Boucle : si une règle n'est pas respectée, on réaffiche le formulaire
    while (dlg.exec() == QDialog::Accepted) {
        if (eNum->text().trimmed().isEmpty() || eDep->text().trimmed().isEmpty()
            || eArr->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Vol", "Numéro, départ et arrivée sont obligatoires.");
            continue;
        }
        if (eDep->text().trimmed().compare(eArr->text().trimmed(), Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "Vol",
                                 "La ville de départ doit être différente de la ville d'arrivée.");
            continue;
        }
        if (dDep->dateTime() <= QDateTime::currentDateTime()) {
            QMessageBox::warning(this, "Vol", "La date de départ doit être dans le futur.");
            continue;
        }
        if (dArr->dateTime() <= dDep->dateTime()) {
            QMessageBox::warning(this, "Vol",
                                 "La date d'arrivée doit être après la date de départ.");
            continue;
        }

        bool ok;
        if (idVol > 0)
            ok = m_db->modifierVol(idVol, eNum->text(), eComp->text(), eDep->text(),
                                   eArr->text(), dDep->dateTime(), dArr->dateTime(),
                                   sPrix->value(), sPlaces->value());
        else
            ok = m_db->ajouterVol(eNum->text(), eComp->text(), eDep->text(),
                                  eArr->text(), dDep->dateTime(), dArr->dateTime(),
                                  sPrix->value(), sPlaces->value());
        if (ok) {
            QMessageBox::information(this, "Vol",
                                     idVol > 0 ? "Vol modifié." : "Vol ajouté.");
            actualiserTout();
            break;
        }
        QMessageBox::warning(this, "Vol",
                             "Enregistrement impossible : ce numéro de vol existe peut-être déjà, "
                             "ou la capacité est inférieure au nombre de sièges déjà réservés.");
    }
}


void AdminDialog::chargerReservations()
{
    QSqlQuery q(m_db->database());
    if (!q.exec("SELECT r.reference, r.passager, v.numero_vol, r.nb_places, "
                "r.prix_total, r.statut, r.date_reservation, "
                "IFNULL(u.email, '(ancien test)') "
                "FROM RESERVATION r "
                "JOIN VOL v ON v.id_vol = r.id_vol "
                "LEFT JOIN UTILISATEUR u ON u.id_utilisateur = r.id_utilisateur "
                "ORDER BY r.id_reservation DESC")) {
        qDebug() << "Erreur SELECT reservations :" << q.lastError().text();
    }
    m_modelRes->setQuery(std::move(q));
    m_modelRes->setHeaderData(0, Qt::Horizontal, "Référence");
    m_modelRes->setHeaderData(1, Qt::Horizontal, "Passager");
    m_modelRes->setHeaderData(2, Qt::Horizontal, "Vol");
    m_modelRes->setHeaderData(3, Qt::Horizontal, "Places");
    m_modelRes->setHeaderData(4, Qt::Horizontal, "Prix total");
    m_modelRes->setHeaderData(5, Qt::Horizontal, "Statut");
    m_modelRes->setHeaderData(6, Qt::Horizontal, "Date");
    m_modelRes->setHeaderData(7, Qt::Horizontal, "Compte client");
    m_tableRes->resizeColumnsToContents();
}

void AdminDialog::changerStatut(const QString &nouveauStatut)
{
    QModelIndex idx = m_tableRes->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Statut", "Sélectionnez d'abord une réservation.");
        return;
    }
    QString ref = m_modelRes->data(m_modelRes->index(idx.row(), 0)).toString();
    QString ancien = m_modelRes->data(m_modelRes->index(idx.row(), 5)).toString();

    if (ancien == nouveauStatut) {
        QMessageBox::information(this, "Statut",
                                 "Cette réservation a déjà ce statut.");
        return;
    }
    if (QMessageBox::question(this, "Statut",
                              "Passer la réservation " + ref + " au statut « "
                                  + nouveauStatut + " » ?") != QMessageBox::Yes)
        return;

    if (m_db->changerStatutReservation(ref, nouveauStatut)) {
        QMessageBox::information(this, "Statut", "Statut mis à jour.");
        actualiserTout();
    } else {
        QMessageBox::warning(this, "Statut",
                             "Changement impossible (par exemple : plus assez de places "
                             "pour re-confirmer cette réservation).");
    }
}


void AdminDialog::simulerPerturbation()
{
    QModelIndex idx = m_tableVols->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Simulation", "Sélectionnez d'abord un vol.");
        return;
    }
    int idVol = m_modelVols->data(m_modelVols->index(idx.row(), 0)).toInt();
    QString numero = m_modelVols->data(m_modelVols->index(idx.row(), 1)).toString();

    bool ok = false;
    QStringList choix = {"Retard", "Annulation", "Rétablir (à l'heure)"};
    QString c = QInputDialog::getItem(this, "Simulation",
                                      "Événement sur le vol " + numero + " :",
                                      choix, 0, false, &ok);
    if (!ok)
        return;
    QString type = (c == "Retard") ? "retard"
                   : (c == "Annulation") ? "annulation" : "normal";
    int minutes = 0;
    if (type == "retard") {
        minutes = QInputDialog::getInt(this, "Retard", "Minutes de retard :",
                                       60, 5, 720, 5, &ok);
        if (!ok)
            return;
    }

    int nb = 0;
    if (m_db->simulerPerturbation(idVol, type, minutes, nb)) {
        QMessageBox::information(this, "Simulation",
                                 "Simulation appliquée au vol " + numero + ".\n"
                                     + QString::number(nb) + " alerte(s) envoyée(s) aux clients concernés.");
        actualiserTout();
    } else {
        QMessageBox::warning(this, "Simulation", "La simulation a échoué.");
    }
}