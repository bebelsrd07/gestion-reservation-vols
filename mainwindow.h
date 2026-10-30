#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlQueryModel>

class DatabaseManager;
class QAction;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DatabaseManager *db, int idUtilisateur,
                        const QString &role, const QString &nom,
                        QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnRechercher_clicked();
    void on_tableVols_doubleClicked(const QModelIndex &index);
    void on_btnReservations_clicked();
    void deconnexion();
    void ouvrirAdmin();
    void voirDetails();
    void ouvrirVoyage();
    void ouvrirEscales();


private:
    void majAlertes(bool notifier = false);
    QAction *m_actVoyage = nullptr;
    void chargerVols(const QString &depart = "", const QString &arrivee = "");
    bool exigerConnexion();

    Ui::MainWindow *ui;
    DatabaseManager *m_dbManager;
    QSqlQueryModel *m_model;
    int m_idUtilisateur;
    QString m_role;
    QString m_nomUtilisateur;
};

#endif // MAINWINDOW_H