#include "mainwindow.h"
#include "databasemanager.h"
#include "logindialog.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DatabaseManager db;
    if (!db.connecter()) {
        QMessageBox::critical(nullptr, "Erreur",
                              "Impossible de se connecter a MySQL.\nVerifiez le serveur et le pilote.");
        return 1;
    }

    LoginDialog login(&db);
    if (login.exec() != QDialog::Accepted)
        return 0;                           // fenêtre fermée : on quitte

    MainWindow w(&db, login.idUtilisateur(), login.role(), login.nomComplet());
    w.show();
    return a.exec();
}