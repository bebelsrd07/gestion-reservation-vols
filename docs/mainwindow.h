#pragma once

#include <QMainWindow>
#include <QString>
#include <optional>
#include "utilisateur.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnRechercher_clicked();
    void on_btnReserver_clicked();
    void on_btnInscription_clicked();
    void on_btnConnexion_clicked();
    void on_btnMesReservations_clicked();

private:
    Ui::MainWindow *ui;
    std::optional<Utilisateur> m_utilisateur;
    void chargerVols(const QString &dep = "", const QString &arr = "");
};