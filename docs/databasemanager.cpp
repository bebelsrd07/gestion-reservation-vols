#include <QUuid>
#include "databasemanager.h"

DatabaseManager::DatabaseManager() {
    m_db = QSqlDatabase::addDatabase("QODBC");
}

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::connecter() {
    m_db.setDatabaseName(
        "DRIVER={MySQL ODBC 26.10 Unicode Driver};"
        "SERVER=localhost;"
        "PORT=3307;"
        "DATABASE=reservation_vol_db;"
        "UID=root;"
        "PWD=170107;"
        "CHARSET=utf8mb4;");

    if (!m_db.open()) {
        qDebug() << "Erreur de connexion MySQL :" << m_db.lastError().text();
        return false;
    }
    qDebug() << "Connexion reussie a MySQL !";
    return true;
}

void DatabaseManager::deconnecter() {
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::estConnecte() const {
    return m_db.isOpen();
}

bool DatabaseManager::emailExiste(const QString &email)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM UTILISATEUR WHERE email = :email");
    q.bindValue(":email", email);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

bool DatabaseManager::inscrireUtilisateur(const QString &nom, const QString &prenom,
                                          const QString &email, const QString &motDePasse)
{
    QSqlQuery q;
    q.prepare("INSERT INTO UTILISATEUR (nom, prenom, email, mot_de_passe, role) "
              "VALUES (:nom, :prenom, :email, :mdp, 'Client')");
    q.bindValue(":nom", nom);
    q.bindValue(":prenom", prenom);
    q.bindValue(":email", email);
    q.bindValue(":mdp", Utilisateur::hacherMotDePasse(motDePasse));

    if (!q.exec()) {
        qDebug() << "Erreur inscription :" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<Utilisateur> DatabaseManager::authentifier(const QString &email,
                                                         const QString &motDePasse)
{
    QSqlQuery q;
    q.prepare("SELECT id_utilisateur, nom, prenom, email, mot_de_passe, role "
              "FROM UTILISATEUR WHERE email = :email");
    q.bindValue(":email", email);

    if (q.exec() && q.next()) {
        Utilisateur u(q.value(0).toInt(), q.value(1).toString(),
                      q.value(2).toString(), q.value(3).toString(),
                      q.value(4).toString(), q.value(5).toString());
        if (u.verifierMotDePasse(motDePasse))
            return u;
    }
    return std::nullopt;
}

bool DatabaseManager::creerReservation(int idUtilisateur, int idVol,
                                       int nombreBillets, QString &reference)
{
    if (nombreBillets <= 0 || !m_db.transaction())
        return false;

    QSqlQuery q;
    q.prepare("SELECT prix, places_disponibles FROM VOL "
              "WHERE id_vol = :id FOR UPDATE");
    q.bindValue(":id", idVol);
    if (!q.exec() || !q.next()) {
        m_db.rollback();
        return false;
    }

    double prix = q.value(0).toDouble();
    int places = q.value(1).toInt();

    if (places < nombreBillets) {   // RG04
        m_db.rollback();
        return false;
    }

    reference = "REF-" + QUuid::createUuid().toString(QUuid::Id128)
                             .left(8).toUpper();   // RG06
    double prixTotal = prix * nombreBillets;       // RG07

    QSqlQuery ins;
    ins.prepare("INSERT INTO RESERVATION "
                "(reference, nombre_billets, prix_total, id_utilisateur, id_vol) "
                "VALUES (:ref, :nb, :total, :user, :vol)");
    ins.bindValue(":ref", reference);
    ins.bindValue(":nb", nombreBillets);
    ins.bindValue(":total", prixTotal);
    ins.bindValue(":user", idUtilisateur);
    ins.bindValue(":vol", idVol);
    if (!ins.exec()) {
        qDebug() << "Erreur INSERT reservation :" << ins.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery upd;                                  // RG05
    upd.prepare("UPDATE VOL SET places_disponibles = places_disponibles - :nb "
                "WHERE id_vol = :vol");
    upd.bindValue(":nb", nombreBillets);
    upd.bindValue(":vol", idVol);
    if (!upd.exec()) {
        qDebug() << "Erreur UPDATE vol :" << upd.lastError().text();
        m_db.rollback();
        return false;
    }

    return m_db.commit();
}

bool DatabaseManager::annulerReservation(int idReservation, int idUtilisateur)
{
    if (!m_db.transaction())
        return false;

    QSqlQuery q;
    q.prepare("SELECT id_vol, nombre_billets FROM RESERVATION "
              "WHERE id_reservation = :id AND id_utilisateur = :user "
              "AND statut = :conf FOR UPDATE");
    q.bindValue(":id", idReservation);
    q.bindValue(":user", idUtilisateur);
    q.bindValue(":conf", "Confirmée");

    if (!q.exec() || !q.next()) {
        m_db.rollback();
        return false;
    }

    int idVol = q.value(0).toInt();
    int nb = q.value(1).toInt();

    QSqlQuery upd;
    upd.prepare("UPDATE RESERVATION SET statut = :ann WHERE id_reservation = :id");
    upd.bindValue(":ann", "Annulée");
    upd.bindValue(":id", idReservation);
    if (!upd.exec()) {
        m_db.rollback();
        return false;
    }

    QSqlQuery rest;                                  // RG05 : on rend les places
    rest.prepare("UPDATE VOL SET places_disponibles = places_disponibles + :nb "
                 "WHERE id_vol = :vol");
    rest.bindValue(":nb", nb);
    rest.bindValue(":vol", idVol);
    if (!rest.exec()) {
        m_db.rollback();
        return false;
    }

    return m_db.commit();
}