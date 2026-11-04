#include "databasemanager.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>
#include <QShortcut>

static QString trouverFichierStyleQss()
{
    QString direct = QDir::currentPath() + QStringLiteral("/style.qss");
    if (QFileInfo::exists(direct))
        return direct;

    QDir dossier(QCoreApplication::applicationDirPath());
    for (int niveau = 0; niveau < 5; ++niveau) {
        QString candidat = dossier.filePath(QStringLiteral("style.qss"));
        if (QFileInfo::exists(candidat))
            return candidat;
        if (!dossier.cdUp())
            break;
    }
    return QString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ReservationBilletsAvion"));

    auto chargerStyleQss = [&app]() {
        QString chemin = trouverFichierStyleQss();
        if (!chemin.isEmpty()) {
            QFile fichier(chemin);
            if (fichier.open(QIODevice::ReadOnly | QIODevice::Text))
                app.setStyleSheet(QString::fromUtf8(fichier.readAll()));
        }
    };
    chargerStyleQss();

    DatabaseManager db;
    if (!db.connecter()) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Connexion MySQL impossible"),
            QStringLiteral("Impossible de se connecter à la base MySQL locale.\n\n"
                           "Détail : %1\n\n"
                           "Vérifiez que MySQL est démarré et ajustez si besoin :\n%2")
                .arg(db.derniereErreur(), DatabaseManager::cheminFichierConfig()));
        return 1;
    }

    MainWindow fenetre(&db);

    // Touche F5 pour recharger style.qss en direct sans recompiler
    auto *raccourciStyle = new QShortcut(QKeySequence(Qt::Key_F5), &fenetre);
    QObject::connect(raccourciStyle, &QShortcut::activated, chargerStyleQss);

    fenetre.show();
    return app.exec();
}
