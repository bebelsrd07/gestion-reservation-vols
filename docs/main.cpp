#include "mainwindow.h"
#include "databasemanager.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qDebug() << QSqlDatabase::drivers();

    if (!DatabaseManager::instance().connecter()) {
        QMessageBox::critical(nullptr, "Erreur", "Connexion à MySQL impossible.");
        return -1;
    }

    MainWindow w;
    w.show();
    return a.exec();
}