#include "dialogmesreservations.h"
#include "databasemanager.h"
#include "dialogreservation.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialoginscription.h"
#include "dialogconnexion.h"
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <utility>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Réservation de billets d'avion");
    chargerVols();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::chargerVols(const QString &dep, const QString &arr)
{
    QSqlQuery query;
    query.prepare("SELECT id_vol, numero_vol, compagnie, ville_depart, ville_arrivee, "
                  "date_depart, prix, places_disponibles FROM VOL "
                  "WHERE ville_depart LIKE :dep AND ville_arrivee LIKE :arr");
    query.bindValue(":dep", "%" + dep + "%");
    query.bindValue(":arr", "%" + arr + "%");

    if (!query.exec()) {
        qDebug() << "Erreur requete :" << query.lastError().text();
        return;
    }

    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(std::move(query));

    model->setHeaderData(1, Qt::Horizontal, "N° Vol");
    model->setHeaderData(2, Qt::Horizontal, "Compagnie");
    model->setHeaderData(3, Qt::Horizontal, "Départ");
    model->setHeaderData(4, Qt::Horizontal, "Arrivée");
    model->setHeaderData(5, Qt::Horizontal, "Date/Heure");
    model->setHeaderData(6, Qt::Horizontal, "Prix");
    model->setHeaderData(7, Qt::Horizontal, "Places");

    ui->tableVols->setModel(model);
    ui->tableVols->setColumnHidden(0, true);
    ui->tableVols->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableVols->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableVols->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void MainWindow::on_btnRechercher_clicked()
{
    chargerVols(ui->txtVilleDepart->text().trimmed(),
                ui->txtVilleArrivee->text().trimmed());
}

void MainWindow::on_btnInscription_clicked()
{
    DialogInscription dlg(this);
    dlg.exec();
}

void MainWindow::on_btnConnexion_clicked()
{
    DialogConnexion dlg(this);
    if (dlg.exec() == QDialog::Accepted && dlg.utilisateur()) {
        m_utilisateur = dlg.utilisateur();
        setWindowTitle("Réservation de billets d'avion - "
                       + m_utilisateur->getPrenom() + " " + m_utilisateur->getNom());
        QMessageBox::information(this, "Bienvenue",
                                 "Bonjour " + m_utilisateur->getPrenom() + " !");
    }
}

void MainWindow::on_btnReserver_clicked()
{
    if (!m_utilisateur) {
        QMessageBox::warning(this, "Connexion requise",
                             "Veuillez vous connecter avant de réserver.");
        return;
    }

    QModelIndex index = ui->tableVols->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Attention", "Veuillez sélectionner un vol.");
        return;
    }

    int row = index.row();
    QAbstractItemModel *model = ui->tableVols->model();
    int idVol = model->index(row, 0).data().toInt();
    QString numeroVol = model->index(row, 1).data().toString();
    double prix = model->index(row, 6).data().toDouble();
    int places = model->index(row, 7).data().toInt();

    if (places <= 0) {
        QMessageBox::warning(this, "Complet", "Il n'y a plus de places sur ce vol.");
        return;
    }

    DialogReservation dlg(numeroVol, prix, places, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString reference;
    if (DatabaseManager::instance().creerReservation(
            m_utilisateur->getIdUtilisateur(), idVol, dlg.nombreBillets(), reference)) {
        QMessageBox::information(this, "Succès",
                                 "Réservation confirmée !\nRéférence : " + reference);
    } else {
        QMessageBox::critical(this, "Erreur", "La réservation a échoué.");
    }

    chargerVols(ui->txtVilleDepart->text().trimmed(),
                ui->txtVilleArrivee->text().trimmed());
}

void MainWindow::on_btnMesReservations_clicked()
{
    if (!m_utilisateur) {
        QMessageBox::warning(this, "Connexion requise",
                             "Veuillez vous connecter pour voir vos réservations.");
        return;
    }

    DialogMesReservations dlg(m_utilisateur->getIdUtilisateur(), this);
    dlg.exec();

    // Les places ont peut-être changé après une annulation
    chargerVols(ui->txtVilleDepart->text().trimmed(),
                ui->txtVilleArrivee->text().trimmed());
}