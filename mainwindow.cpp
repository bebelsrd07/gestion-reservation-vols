#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QInputDialog>
#include <QMessageBox>
#include "reservation.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QTableView>
#include <QPushButton>
#include <QSqlQueryModel>
#include <QSqlError>
#include <QDebug>
#include <QDate>
#include <QToolBar>
#include <QAction>
#include <QProcess>
#include <QApplication>
#include "admindialog.h"
#include <QDateTime>
#include "escaledialog.h"
#include "voyagedialog.h"
#include <QTimer>
#include "seatmapdialog.h"

MainWindow::MainWindow(DatabaseManager *db, int idUtilisateur,
                       const QString &role, const QString &nom, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    m_dbManager(db), m_model(new QSqlQueryModel(this)),
    m_idUtilisateur(idUtilisateur), m_role(role), m_nomUtilisateur(nom)
{
    ui->setupUi(this);
    setWindowTitle("Réservation de vols - " + m_nomUtilisateur + " (" + m_role + ")");
    QToolBar *barre = addToolBar("Compte");
    barre->setMovable(false);
    QAction *actEscales = barre->addAction("Vols avec escale");
    connect(actEscales, &QAction::triggered, this, &MainWindow::ouvrirEscales);
    if (m_idUtilisateur > 0) {
        m_actVoyage = barre->addAction("Mon voyage");
        connect(m_actVoyage, &QAction::triggered, this, &MainWindow::ouvrirVoyage);
        QTimer *minuteur = new QTimer(this);
        connect(minuteur, &QTimer::timeout, this, [this]() { majAlertes(); });
        minuteur->start(30000);                         // toutes les 30 secondes
        QTimer::singleShot(400, this, [this]() { majAlertes(true); });
    }
    QAction *actDetails = barre->addAction("Détails du vol");
    connect(actDetails, &QAction::triggered, this, &MainWindow::voirDetails);
    QAction *actDeco = barre->addAction(m_role == "visiteur"
                                            ? "Se connecter / S'inscrire"
                                            : "Se déconnecter");
    connect(actDeco, &QAction::triggered, this, &MainWindow::deconnexion);
    if (m_role == "admin") {
        QAction *actAdmin = barre->addAction("Administration");
        connect(actAdmin, &QAction::triggered, this, &MainWindow::ouvrirAdmin);
    }
    ui->dateDepart->setDate(QDate::currentDate());
    ui->dateDepart->setMinimumDate(QDate::currentDate());
    ui->dateDepart->setDisplayFormat("dd/MM/yyyy");
    ui->tableVols->setModel(m_model);
    ui->tableVols->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableVols->setEditTriggers(QAbstractItemView::NoEditTriggers);
    chargerVols();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::chargerVols(const QString &depart, const QString &arrivee)
{
    QString sql = "SELECT numero_vol, ville_depart, ville_arrivee, prix, "
                  "places_disponibles, id_vol, compagnie, date_depart "
                  "FROM VOL "
                  "WHERE ville_depart LIKE :d AND ville_arrivee LIKE :a "
                  "AND places_disponibles >= :nb "
                                    "AND date_depart > NOW() AND statut_vol <> 'annule' ";
    if (ui->chkDate->isChecked())
        sql += "AND DATE(date_depart) = :dt ";
    sql += "ORDER BY date_depart, prix";

    QSqlQuery q(m_dbManager->database());
    q.prepare(sql);
    q.bindValue(":d", "%" + depart + "%");
    q.bindValue(":a", "%" + arrivee + "%");
    q.bindValue(":nb", ui->spinPassagers->value());
    if (ui->chkDate->isChecked())
        q.bindValue(":dt", ui->dateDepart->date().toString("yyyy-MM-dd"));
    q.exec();
    m_model->setQuery(std::move(q));

    m_model->setHeaderData(0, Qt::Horizontal, "N° vol");
    m_model->setHeaderData(1, Qt::Horizontal, "Départ");
    m_model->setHeaderData(2, Qt::Horizontal, "Arrivée");
    m_model->setHeaderData(3, Qt::Horizontal, "Prix (EUR)");
    m_model->setHeaderData(4, Qt::Horizontal, "Places");
    m_model->setHeaderData(6, Qt::Horizontal, "Compagnie");
    m_model->setHeaderData(7, Qt::Horizontal, "Date de départ");
    ui->tableVols->setColumnHidden(5, true);
}

void MainWindow::on_btnRechercher_clicked()
{
    if (ui->chkDate->isChecked() && ui->dateDepart->date() < QDate::currentDate()) {
        QMessageBox::warning(this, "Recherche",
                             "La date de départ ne peut pas être dans le passé.");
        return;
    }
    chargerVols(ui->editDepart->text().trimmed(),
                ui->editArrivee->text().trimmed());

    if (!ui->editDepart->text().trimmed().isEmpty()
        && ui->editDepart->text().trimmed().compare(
               ui->editArrivee->text().trimmed(), Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, "Recherche",
                             "La ville de départ doit être différente de la ville d'arrivée.");
        return;
    }
    if (m_model->rowCount() == 0)
        QMessageBox::information(this, "Recherche",
                                 "Aucun vol disponible pour ces critères.");
}
void MainWindow::on_tableVols_doubleClicked(const QModelIndex &index)
{
    if (!exigerConnexion())
        return;

    int row = index.row();
    int idVol = m_model->data(m_model->index(row, 5)).toInt();
    QString numero = m_model->data(m_model->index(row, 0)).toString();
    QString depart = m_model->data(m_model->index(row, 1)).toString();
    QString arrivee = m_model->data(m_model->index(row, 2)).toString();
    double prix = m_model->data(m_model->index(row, 3)).toDouble();
    int places = m_model->data(m_model->index(row, 4)).toInt();

    bool ok = false;
    QString passager = QInputDialog::getText(this, "Réservation",
                                             "Nom du passager :", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || passager.isEmpty()) {
        QMessageBox::warning(this, "Réservation", "Le nom du passager est obligatoire.");
        return;
    }

    int nb = QInputDialog::getInt(this, "Réservation",
                                  "Nombre de places (max " + QString::number(places) + ") :",
                                  qMin(ui->spinPassagers->value(), places), 1, places, 1, &ok);
    if (!ok)
        return;

    // Choix des sièges sur le plan de la cabine
    SeatMapDialog plan(m_dbManager, idVol, nb,
                       "Vol " + numero + " : " + depart + " → " + arrivee, this);
    if (plan.exec() != QDialog::Accepted)
        return;

    Reservation r(idVol, passager, nb, prix);
    if (m_dbManager->enregistrerReservation(r, m_idUtilisateur, plan.sieges())) {
        // Prix final (avec suppléments) et numéros de sièges, lus dans la base
        double total = r.prixTotal();
        QString sieges;
        QSqlQuery q(m_dbManager->database());
        q.prepare("SELECT r.prix_total, "
                  "GROUP_CONCAT(s.numero ORDER BY s.rangee, s.lettre SEPARATOR ', ') "
                  "FROM RESERVATION r LEFT JOIN SIEGE s ON s.id_reservation = r.id_reservation "
                  "WHERE r.reference = :ref GROUP BY r.id_reservation, r.prix_total");
        q.bindValue(":ref", r.reference());
        if (q.exec() && q.next()) {
            total = q.value(0).toDouble();
            sieges = q.value(1).toString();
        }
        QMessageBox::information(this, "Réservation confirmée",
                                 "Vol : " + numero + " (" + depart + " → " + arrivee + ")\n"
                                                                                       "Passager : " + passager + "\n"
                                                  "Sièges : " + sieges + "\n"
                                                "Référence : " + r.reference() + "\n"
                                                       "Prix total : " + QString::number(total, 'f', 2) + " EUR");
        chargerVols(ui->editDepart->text().trimmed(),
                    ui->editArrivee->text().trimmed());
    } else {
        QMessageBox::critical(this, "Réservation",
                              "Impossible d'enregistrer : un siège vient peut-être d'être pris, "
                              "plus assez de places, vol déjà parti ou erreur de base.");
        chargerVols(ui->editDepart->text().trimmed(),
                    ui->editArrivee->text().trimmed());
    }
}


void MainWindow::on_btnReservations_clicked()
{
    if (!exigerConnexion())
        return;
    QDialog dlg(this);
    if (m_role == "visiteur")
        setWindowTitle("Réservation de vols - Visiteur (consultation seule)");
    else
        setWindowTitle("Réservation de vols - " + m_nomUtilisateur + " (" + m_role + ")");
    dlg.resize(650, 350);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QTableView *table = new QTableView(&dlg);
    QPushButton *btnAnnuler = new QPushButton("Annuler la réservation", &dlg);
    layout->addWidget(table);
    layout->addWidget(btnAnnuler);

    QSqlQueryModel *model = new QSqlQueryModel(&dlg);

        auto recharger = [this, model]() {
                QSqlQuery q(QSqlDatabase::database("connexion_principale"));
        q.prepare("SELECT reference, passager, nb_places, prix_total, statut "
                  "FROM RESERVATION WHERE id_utilisateur = :u "
                  "ORDER BY id_reservation DESC");
        q.bindValue(":u", m_idUtilisateur);
        if (!q.exec()) {
                qDebug() << "Erreur SELECT :" << q.lastError().text();
            }

            model->setQuery(std::move(q));
            model->setHeaderData(0, Qt::Horizontal, "Référence");
            model->setHeaderData(1, Qt::Horizontal, "Passager");
            model->setHeaderData(2, Qt::Horizontal, "Places");
            model->setHeaderData(3, Qt::Horizontal, "Prix total");
        model->setHeaderData(4, Qt::Horizontal, "Statut");
    };
            recharger();
            table->setModel(model);


    connect(btnAnnuler, &QPushButton::clicked, &dlg, [&]() {
        QModelIndex idx = table->currentIndex();
        if (!idx.isValid()) {
            QMessageBox::warning(&dlg, "Annulation",
                                 "Sélectionnez d'abord une réservation.");
            return;
        }
        QString ref = model->data(model->index(idx.row(), 0)).toString();
        QString statut = model->data(model->index(idx.row(), 4)).toString();
        if (statut == "annulee") {
            QMessageBox::information(&dlg, "Annulation",
                                     "Cette réservation est déjà annulée.");
            return;
        }
        if (QMessageBox::question(&dlg, "Annulation",
                                  "Annuler la réservation " + ref + " ?") != QMessageBox::Yes)
            return;

        if (m_dbManager->annulerReservation(ref)) {
            QMessageBox::information(&dlg, "Annulation",
                                     "Réservation annulée, places remises en stock.");
            recharger();
            chargerVols(ui->editDepart->text().trimmed(),
                        ui->editArrivee->text().trimmed());
        } else {
            QMessageBox::critical(&dlg, "Annulation",
                                  "Impossible d'annuler la réservation.");
        }
    });

    dlg.exec();
}


void MainWindow::deconnexion()
{
    if (m_role != "visiteur"
        && QMessageBox::question(this, "Déconnexion",
                                 "Voulez-vous vraiment vous déconnecter ?") != QMessageBox::Yes)
        return;
    // On relance l'application : la fenêtre de connexion réapparaît
    QProcess::startDetached(QApplication::applicationFilePath(), QStringList());
    qApp->quit();
}

void MainWindow::ouvrirAdmin()
{
    if (m_role != "admin") {
        QMessageBox::warning(this, "Accès refusé",
                             "Cet espace est réservé aux administrateurs.");
        return;
    }
    AdminDialog dlg(m_dbManager, this);
    dlg.exec();
    // Les vols ont pu changer : on rafraîchit la recherche
    chargerVols(ui->editDepart->text().trimmed(),
                ui->editArrivee->text().trimmed());
}


bool MainWindow::exigerConnexion()
{
    if (m_role != "visiteur")
        return true;
    if (QMessageBox::question(this, "Connexion requise",
                              "Vous devez avoir un compte pour réserver ou consulter "
                              "vos réservations.\n\n"
                              "Ouvrir la fenêtre de connexion / inscription ?")
        == QMessageBox::Yes)
        deconnexion();                      // relance l'application : fenêtre Accès
    return false;
}


void MainWindow::voirDetails()
{
    QModelIndex idx = ui->tableVols->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::information(this, "Détails du vol",
                                 "Cliquez d'abord sur un vol dans la liste.");
        return;
    }
    int idVol = m_model->data(m_model->index(idx.row(), 5)).toInt();

    QSqlQuery q(m_dbManager->database());
    q.prepare("SELECT numero_vol, compagnie, ville_depart, ville_arrivee, "
              "date_depart, date_arrivee, prix, places_disponibles "
              "FROM VOL WHERE id_vol = :id");
    q.bindValue(":id", idVol);
    if (!q.exec() || !q.next()) {
        QMessageBox::warning(this, "Détails du vol", "Vol introuvable.");
        return;
    }
    QDateTime dd = q.value(4).toDateTime();
    QDateTime da = q.value(5).toDateTime();
    qint64 minutes = dd.secsTo(da) / 60;
    QString duree = QString::number(minutes / 60) + " h "
                    + QString("%1").arg(minutes % 60, 2, 10, QChar('0'));

    QMessageBox::information(this, "Détails du vol " + q.value(0).toString(),
                             "Compagnie : " + q.value(1).toString() + "\n"
                                                                      "Trajet : " + q.value(2).toString() + " → " + q.value(3).toString() + "\n"
                                                                                           "Départ : " + dd.toString("dd/MM/yyyy HH:mm") + "\n"
                                                                     "Arrivée : " + da.toString("dd/MM/yyyy HH:mm") + "\n"
                                                                     "Durée : " + duree + "\n"
                                           "Prix par billet : " + QString::number(q.value(6).toDouble(), 'f', 2) + " EUR\n"
                                                                                    "Places restantes : " + q.value(7).toString());
}


void MainWindow::ouvrirEscales()
{
        EscaleDialog dlg(m_dbManager, m_idUtilisateur, this);
    dlg.exec();
}


void MainWindow::majAlertes(bool notifier)
{
    if (!m_actVoyage)
        return;
    int n = m_dbManager->nombreAlertesNonLues(m_idUtilisateur);
    m_actVoyage->setText(n > 0 ? "Mon voyage (" + QString::number(n) + " alerte(s))"
                               : "Mon voyage");
    if (notifier && n > 0)
        QMessageBox::warning(this, "Alertes voyage",
                             "Vous avez " + QString::number(n)
                                 + " alerte(s) non lue(s) sur vos voyages.\n"
                                   "Ouvrez « Mon voyage » pour les consulter.");
}

void MainWindow::ouvrirVoyage()
{
    VoyageDialog dlg(m_dbManager, m_idUtilisateur, this);
    dlg.exec();
    majAlertes();
}