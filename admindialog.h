#ifndef ADMINDIALOG_H
#define ADMINDIALOG_H

#include <QDialog>

class DatabaseManager;
class QTabWidget;
class QTableView;
class QSqlQueryModel;
class QLabel;

class AdminDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AdminDialog(DatabaseManager *db, QWidget *parent = nullptr);

private slots:
    void actualiserTout();
    void ajouterVol();
    void modifierVol();
    void supprimerVol();
    void simulerPerturbation();

private:
    void formulaireVol(int idVol);          // idVol = 0 : ajout, sinon modification
    void chargerVols();
    void chargerStatistiques();
    void chargerReservations();
    void changerStatut(const QString &nouveauStatut);

    DatabaseManager *m_db;
    QTabWidget *m_onglets;

    QLabel *m_lblVols;
    QLabel *m_lblTotal;
    QLabel *m_lblConf;
    QLabel *m_lblAnn;

    QTableView *m_tableVols;
    QSqlQueryModel *m_modelVols;
    QTableView *m_tableRes;
    QSqlQueryModel *m_modelRes;
};

#endif // ADMINDIALOG_H