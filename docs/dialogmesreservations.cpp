#include "dialogmesreservations.h"
#include "ui_dialogmesreservations.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <utility>

DialogMesReservations::DialogMesReservations(int idUtilisateur, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogMesReservations)
    , m_idUtilisateur(idUtilisateur)
{
    ui->setupUi(this);
    setWindowTitle("Mes réservations");
    resize(850, 400);
    charger();
}

DialogMesReservations::~DialogMesReservations()
{
    delete ui;
}

void DialogMesReservations::charger()
{
    QSqlQuery query;
    query.prepare("SELECT r.id_reservation, r.reference, v.numero_vol, "
                  "v.ville_depart, v.ville_arrivee, r.nombre_billets, "
                  "r.prix_total, r.statut "
                  "FROM RESERVATION r JOIN VOL v ON r.id_vol = v.id_vol "
                  "WHERE r.id_utilisateur = :user "
                  "ORDER BY r.date_reservation DESC");
    query.bindValue(":user", m_idUtilisateur);

    if (!query.exec()) {
        qDebug() << "Erreur requete reservations :" << query.lastError().text();
        return;
    }

    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));

    model->setHeaderData(1, Qt::Horizontal, "Référence");
    model->setHeaderData(2, Qt::Horizontal, "N° Vol");
    model->setHeaderData(3, Qt::Horizontal, "Départ");
    model->setHeaderData(4, Qt::Horizontal, "Arrivée");
    model->setHeaderData(5, Qt::Horizontal, "Billets");
    model->setHeaderData(6, Qt::Horizontal, "Total");
    model->setHeaderData(7, Qt::Horizontal, "Statut");

    ui->tableReservations->setModel(model);
    ui->tableReservations->setColumnHidden(0, true);
    ui->tableReservations->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableReservations->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableReservations->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void DialogMesReservations::on_btnAnnuler_clicked()
{
    QModelIndex index = ui->tableReservations->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Attention", "Veuillez sélectionner une réservation.");
        return;
    }

    QAbstractItemModel *model = ui->tableReservations->model();
    int idReservation = model->index(index.row(), 0).data().toInt();
    QString reference = model->index(index.row(), 1).data().toString();

    auto rep = QMessageBox::question(this, "Confirmation",
                                     "Annuler la réservation " + reference + " ?");
    if (rep != QMessageBox::Yes)
        return;

    if (DatabaseManager::instance().annulerReservation(idReservation, m_idUtilisateur)) {
        QMessageBox::information(this, "Succès", "Réservation annulée. Les places ont été restituées.");
        charger();
    } else {
        QMessageBox::warning(this, "Impossible",
                             "Cette réservation est déjà annulée ou n'existe plus.");
    }
}