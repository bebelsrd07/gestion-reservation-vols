#include "databasemanager.h"
#include <QSqlError>
#include <QDebug>
#include <QSqlQuery>
#include <QVariant>
#include <QCryptographicHash>
#include <QUuid>

DatabaseManager::DatabaseManager()
{
    m_db = QSqlDatabase::addDatabase("QMYSQL", "connexion_principale");
    m_db.setHostName("localhost");
    m_db.setPort(3306);
    m_db.setDatabaseName("reservation_vol_db");
    m_db.setUserName("root");
    m_db.setPassword("ngiama4540013");
}

DatabaseManager::~DatabaseManager()
{
    deconnecter();
}

bool DatabaseManager::connecter()
{
    if (!m_db.open()) {
        qDebug() << "Echec connexion MySQL :" << m_db.lastError().text();
        return false;
    }
    qDebug() << "Connexion MySQL reussie !";
    return true;
}

void DatabaseManager::deconnecter()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::estConnecte() const
{
    return m_db.isOpen();
}

QList<Vol> DatabaseManager::obtenirTousLesVols()
{
    QList<Vol> liste;
    QSqlQuery q(m_db);

    if (!q.exec("SELECT id_vol, numero_vol, ville_depart, ville_arrivee, "
                "prix, places_disponibles FROM VOL ORDER BY ville_depart")) {
        qDebug() << "Erreur SELECT :" << q.lastError().text();
        return liste;
    }

    while (q.next()) {
        liste.append(Vol(q.value("id_vol").toInt(),
                         q.value("numero_vol").toString(),
                         q.value("ville_depart").toString(),
                         q.value("ville_arrivee").toString(),
                         q.value("prix").toDouble(),
                         q.value("places_disponibles").toInt()));
    }
    return liste;
}

bool DatabaseManager::enregistrerReservation(const Reservation &r, int idUtilisateur,
                                             const QList<int> &sieges)
{
    if (idUtilisateur <= 0)
        return false;                       // un visiteur ne peut pas réserver
    if (!m_db.transaction())
        return false;
    QSqlQuery q(m_db);

    q.prepare("UPDATE VOL SET places_disponibles = places_disponibles - :n "
              "WHERE id_vol = :id AND places_disponibles >= :n "
              "AND date_depart > NOW() AND statut_vol <> 'annule'");
    q.bindValue(":n", r.nbPlaces());
    q.bindValue(":id", r.idVol());
    if (!q.exec() || q.numRowsAffected() != 1) {
        m_db.rollback();
        return false;
    }
    q.finish();

    q.prepare("INSERT INTO RESERVATION (reference, id_vol, passager, "
              "nb_places, prix_total, id_utilisateur) "
              "VALUES (:ref, :id, :p, :n, :t, :u)");
    q.bindValue(":ref", r.reference());
    q.bindValue(":id", r.idVol());
    q.bindValue(":p", r.passager());
    q.bindValue(":n", r.nbPlaces());
    q.bindValue(":t", r.prixTotal());
    q.bindValue(":u", idUtilisateur);
    if (!q.exec()) {
        m_db.rollback();
        return false;
    }
    int idRes = q.lastInsertId().toInt();
    q.finish();

    if (!attribuerSieges(idRes, r.idVol(), r.nbPlaces(), sieges)) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool DatabaseManager::annulerReservation(const QString &reference)
{
    m_db.transaction();
    QSqlQuery q(m_db);

    q.prepare("SELECT id_vol, nb_places, id_reservation FROM RESERVATION "
              "WHERE reference = :ref AND statut = 'confirmee'");
    q.bindValue(":ref", reference);
    if (!q.exec() || !q.next()) {
        qDebug() << "Annulation etape 1 :" << q.lastError().text()
        << "ref =" << reference;
        m_db.rollback();
        return false;
    }
    int idVol = q.value(0).toInt();
    int nb = q.value(1).toInt();
    int idRes = q.value(2).toInt();
    q.finish();

    q.prepare("UPDATE VOL SET places_disponibles = places_disponibles + :n "
              "WHERE id_vol = :id");
    q.bindValue(":n", nb);
    q.bindValue(":id", idVol);
    if (!q.exec()) {
        qDebug() << "Annulation etape 2 :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.finish();

    q.prepare("UPDATE RESERVATION SET statut = 'annulee' WHERE reference = :ref");
    q.bindValue(":ref", reference);
    if (!q.exec() || q.numRowsAffected() != 1) {
        qDebug() << "Annulation etape 3 :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.finish();

    // Les sièges redeviennent libres
    q.prepare("UPDATE SIEGE SET id_reservation = NULL WHERE id_reservation = :r");
    q.bindValue(":r", idRes);
    if (!q.exec()) {
        qDebug() << "Annulation etape 4 :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool DatabaseManager::ajouterVol(const QString &numero, const QString &compagnie,
                                 const QString &depart, const QString &arrivee,
                                 const QDateTime &dateDepart, const QDateTime &dateArrivee,
                                 double prix, int places)
{
    // Règles de gestion du cahier des charges
    if (numero.trimmed().isEmpty() || depart.trimmed().isEmpty() || arrivee.trimmed().isEmpty())
        return false;
    if (depart.trimmed().compare(arrivee.trimmed(), Qt::CaseInsensitive) == 0)
        return false;                       // départ différent de l'arrivée
    if (prix <= 0 || places < 0 || places > 600 || dateArrivee <= dateDepart)
        return false;

    QSqlQuery q(m_db);

    // Numéro de vol unique
    q.prepare("SELECT COUNT(*) FROM VOL WHERE numero_vol = :num");
    q.bindValue(":num", numero.trimmed());
    if (!q.exec() || !q.next() || q.value(0).toInt() > 0) {
        qDebug() << "Numero de vol deja utilise ou erreur :" << q.lastError().text();
        return false;
    }
    q.finish();

    if (!m_db.transaction())
        return false;
    q.prepare("INSERT INTO VOL (numero_vol, compagnie, ville_depart, ville_arrivee, "
              "date_depart, date_arrivee, prix, places_disponibles, capacite) "
              "VALUES (:num, :comp, :dep, :arr, :dd, :da, :prix, :places, :cap)");
    q.bindValue(":num", numero.trimmed());
    q.bindValue(":comp", compagnie.trimmed());
    q.bindValue(":dep", depart.trimmed());
    q.bindValue(":arr", arrivee.trimmed());
    q.bindValue(":dd", dateDepart);
    q.bindValue(":da", dateArrivee);
    q.bindValue(":prix", prix);
    q.bindValue(":places", places);
    q.bindValue(":cap", places);
    if (!q.exec()) {
        qDebug() << "Erreur ajouterVol :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    int idVol = q.lastInsertId().toInt();
    q.finish();

    if (!genererSieges(idVol, 1, places)) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool DatabaseManager::modifierVol(int idVol, const QString &numero, const QString &compagnie,
                                  const QString &depart, const QString &arrivee,
                                  const QDateTime &dateDepart, const QDateTime &dateArrivee,
                                  double prix, int places)
{
    if (numero.trimmed().isEmpty() || depart.trimmed().isEmpty() || arrivee.trimmed().isEmpty())
        return false;
    if (depart.trimmed().compare(arrivee.trimmed(), Qt::CaseInsensitive) == 0)
        return false;
    if (prix <= 0 || places < 0 || places > 600 || dateArrivee <= dateDepart)
        return false;

    QSqlQuery q(m_db);

    // Numéro unique, en excluant le vol lui-même
    q.prepare("SELECT COUNT(*) FROM VOL WHERE numero_vol = :num AND id_vol <> :id");
    q.bindValue(":num", numero.trimmed());
    q.bindValue(":id", idVol);
    if (!q.exec() || !q.next() || q.value(0).toInt() > 0) {
        qDebug() << "Numero de vol deja utilise ou erreur :" << q.lastError().text();
        return false;
    }
    q.finish();

    if (!m_db.transaction())
        return false;

    // Capacité actuelle et sièges déjà réservés
    q.prepare("SELECT capacite, "
              "(SELECT COUNT(*) FROM SIEGE WHERE id_vol = :id1 AND id_reservation IS NOT NULL) "
              "FROM VOL WHERE id_vol = :id2 FOR UPDATE");
    q.bindValue(":id1", idVol);
    q.bindValue(":id2", idVol);
    if (!q.exec() || !q.next()) {
        m_db.rollback();
        return false;
    }
    int ancienneCapacite = q.value(0).toInt();
    int occupes = q.value(1).toInt();
    q.finish();

    if (places < occupes) {
        qDebug() << "Capacite inferieure aux sieges deja reserves";
        m_db.rollback();
        return false;
    }

    q.prepare("UPDATE VOL SET numero_vol = :num, compagnie = :comp, ville_depart = :dep, "
              "ville_arrivee = :arr, date_depart = :dd, date_arrivee = :da, "
              "prix = :prix, capacite = :cap, places_disponibles = :libres "
              "WHERE id_vol = :id");
    q.bindValue(":num", numero.trimmed());
    q.bindValue(":comp", compagnie.trimmed());
    q.bindValue(":dep", depart.trimmed());
    q.bindValue(":arr", arrivee.trimmed());
    q.bindValue(":dd", dateDepart);
    q.bindValue(":da", dateArrivee);
    q.bindValue(":prix", prix);
    q.bindValue(":cap", places);
    q.bindValue(":libres", places - occupes);
    q.bindValue(":id", idVol);
    if (!q.exec()) {
        qDebug() << "Erreur modifierVol :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.finish();

    if (places > ancienneCapacite) {
        if (!genererSieges(idVol, ancienneCapacite + 1, places)) {
            m_db.rollback();
            return false;
        }
    } else if (places < ancienneCapacite) {
        // On retire des sièges LIBRES (les derniers créés en premier)
        int aRetirer = ancienneCapacite - places;
        q.prepare("DELETE FROM SIEGE WHERE id_vol = :id AND id_reservation IS NULL "
                  "ORDER BY id_siege DESC LIMIT " + QString::number(aRetirer));
        q.bindValue(":id", idVol);
        if (!q.exec()) {
            qDebug() << "Erreur retrait sieges :" << q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

bool DatabaseManager::supprimerVol(int idVol)
{
    QSqlQuery q(m_db);

    // On refuse de supprimer un vol qui a des réservations
    q.prepare("SELECT COUNT(*) FROM RESERVATION WHERE id_vol = :id");
    q.bindValue(":id", idVol);
    if (!q.exec() || !q.next() || q.value(0).toInt() > 0) {
        qDebug() << "Suppression refusee : vol deja reserve ou erreur" << q.lastError().text();
        return false;
    }
    q.finish();

    if (!m_db.transaction())
        return false;
    q.prepare("DELETE FROM SIEGE WHERE id_vol = :id");
    q.bindValue(":id", idVol);
    if (!q.exec()) {
        m_db.rollback();
        return false;
    }
    q.finish();

    q.prepare("DELETE FROM VOL WHERE id_vol = :id");
    q.bindValue(":id", idVol);
    if (!q.exec() || q.numRowsAffected() != 1) {
        qDebug() << "Erreur supprimerVol :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool DatabaseManager::changerStatutReservation(const QString &reference,
                                               const QString &nouveauStatut)
{
    if (nouveauStatut != "confirmee" && nouveauStatut != "annulee")
        return false;
    if (!m_db.transaction())
        return false;

    QSqlQuery q(m_db);
    q.prepare("SELECT id_vol, nb_places, statut, id_reservation FROM RESERVATION "
              "WHERE reference = :ref FOR UPDATE");
    q.bindValue(":ref", reference);
    if (!q.exec() || !q.next()) {
        qDebug() << "Reservation introuvable :" << reference << q.lastError().text();
        m_db.rollback();
        return false;
    }
    int idVol = q.value(0).toInt();
    int nb = q.value(1).toInt();
    QString ancien = q.value(2).toString();
    int idRes = q.value(3).toInt();
    q.finish();

    if (ancien == nouveauStatut) {          // rien à changer
        m_db.rollback();
        return true;
    }

    // Annulation : on rend les places. Re-confirmation : on les reprend (si assez).
    int delta = (nouveauStatut == "annulee") ? nb : -nb;
    q.prepare("UPDATE VOL SET places_disponibles = places_disponibles + :d "
              "WHERE id_vol = :id AND places_disponibles + :d2 >= 0");
    q.bindValue(":d", delta);
    q.bindValue(":d2", delta);
    q.bindValue(":id", idVol);
    if (!q.exec() || q.numRowsAffected() != 1) {
        qDebug() << "Mise a jour des places impossible :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.finish();

    q.prepare("UPDATE RESERVATION SET statut = :s WHERE reference = :ref");
    q.bindValue(":s", nouveauStatut);
    q.bindValue(":ref", reference);
    if (!q.exec()) {
        qDebug() << "Erreur changement statut :" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.finish();

    // Sièges : libérés à l'annulation, repris automatiquement à la re-confirmation
    if (nouveauStatut == "annulee") {
        q.prepare("UPDATE SIEGE SET id_reservation = NULL WHERE id_reservation = :r");
        q.bindValue(":r", idRes);
        if (!q.exec()) {
            m_db.rollback();
            return false;
        }
    } else if (!attribuerSieges(idRes, idVol, nb, QList<int>(), false)) {
        qDebug() << "Re-confirmation : plus assez de sieges libres";
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

int DatabaseManager::nombreVols()
{
    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM VOL") && q.next())
        return q.value(0).toInt();
    return 0;
}

int DatabaseManager::nombreReservations(const QString &statut)
{
    QSqlQuery q(m_db);
    if (statut.isEmpty()) {
        q.prepare("SELECT COUNT(*) FROM RESERVATION");
    } else {
        q.prepare("SELECT COUNT(*) FROM RESERVATION WHERE statut = :s");
        q.bindValue(":s", statut);
    }
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

// ===================== COMPTES UTILISATEURS =====================

QString DatabaseManager::genererSel()
{
    // Texte aléatoire unique (32 caractères)
    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
}

QString DatabaseManager::hacherMotDePasse(const QString &motDePasse, const QString &sel)
{
    QByteArray donnees = (sel + motDePasse).toUtf8();
    QByteArray empreinte = QCryptographicHash::hash(donnees, QCryptographicHash::Sha256);
    return QString::fromLatin1(empreinte.toHex());
}

bool DatabaseManager::inscrireUtilisateur(const QString &nom, const QString &prenom,
                                          const QString &email, const QString &motDePasse,
                                          const QString &role)
{
    QString mail = email.trimmed().toLower();

    // Validations
    if (nom.trimmed().isEmpty() || prenom.trimmed().isEmpty() || mail.isEmpty())
        return false;
    if (!mail.contains('@') || !mail.contains('.'))
        return false;
    if (motDePasse.length() < 6)
        return false;
    if (role != "client" && role != "admin")
        return false;

    QSqlQuery q(m_db);

    // Email unique
    q.prepare("SELECT COUNT(*) FROM UTILISATEUR WHERE email = :e");
    q.bindValue(":e", mail);
    if (!q.exec() || !q.next() || q.value(0).toInt() > 0) {
        qDebug() << "Email deja utilise ou erreur :" << q.lastError().text();
        return false;
    }
    q.finish();

    // On stocke  sel$empreinte
    QString sel = genererSel();
    QString stocke = sel + "$" + hacherMotDePasse(motDePasse, sel);

    q.prepare("INSERT INTO UTILISATEUR (nom, prenom, email, mot_de_passe, role) "
              "VALUES (:n, :p, :e, :m, :r)");
    q.bindValue(":n", nom.trimmed());
    q.bindValue(":p", prenom.trimmed());
    q.bindValue(":e", mail);
    q.bindValue(":m", stocke);
    q.bindValue(":r", role);
    if (!q.exec()) {
        qDebug() << "Erreur inscription :" << q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::connecterUtilisateur(const QString &email, const QString &motDePasse,
                                           int &idUtilisateur, QString &role,
                                           QString &nomComplet)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id_utilisateur, nom, prenom, mot_de_passe, role "
              "FROM UTILISATEUR WHERE email = :e");
    q.bindValue(":e", email.trimmed().toLower());
    if (!q.exec() || !q.next())
        return false;                       // email inconnu

    QString stocke = q.value(3).toString();
    int pos = stocke.indexOf('$');
    if (pos < 0)
        return false;

    QString sel = stocke.left(pos);
    QString empreinteAttendue = stocke.mid(pos + 1);

    if (hacherMotDePasse(motDePasse, sel) != empreinteAttendue)
        return false;                       // mauvais mot de passe

    idUtilisateur = q.value(0).toInt();
    nomComplet = q.value(2).toString() + " " + q.value(1).toString();
    role = q.value(4).toString();
    return true;
}


bool DatabaseManager::enregistrerItineraire(int idVol1, int idVol2,
                                            const QString &passager, int nb,
                                            const QString &typeBillet,
                                            int idUtilisateur, QString &refGroupe,
                                            const QList<int> &sieges1,
                                            const QList<int> &sieges2)
{
    if (idUtilisateur <= 0 || nb <= 0 || passager.trimmed().isEmpty())
        return false;                       // un visiteur ne peut pas réserver
    if (typeBillet != "unique" && typeBillet != "separe")
        return false;
    if (!m_db.transaction())
        return false;

    refGroupe = "ITI-" + QUuid::createUuid().toString(QUuid::WithoutBraces)
                             .remove('-').left(8).toUpper();
    // Séparés : on garde la date et l'heure où l'avertissement a été accepté
    QString dateAvert = (typeBillet == "separe") ? "NOW()" : "NULL";

    const int ids[2] = { idVol1, idVol2 };
    const QList<int> *choix[2] = { &sieges1, &sieges2 };
    for (int i = 0; i < 2; ++i) {
        QSqlQuery q(m_db);
        q.prepare("UPDATE VOL SET places_disponibles = places_disponibles - :n "
                  "WHERE id_vol = :id AND places_disponibles >= :n "
                  "AND date_depart > NOW() AND statut_vol <> 'annule'");
        q.bindValue(":n", nb);
        q.bindValue(":id", ids[i]);
        if (!q.exec() || q.numRowsAffected() != 1) {
            qDebug() << "Itineraire : places ou date, vol" << ids[i]
                     << q.lastError().text();
            m_db.rollback();
            return false;
        }
        q.finish();

        QString ref = "RES-" + QUuid::createUuid().toString(QUuid::WithoutBraces)
                                   .remove('-').left(8).toUpper();
        // Le prix vient de la base (prix du vol x nombre de places), pas de l'écran
        q.prepare("INSERT INTO RESERVATION (reference, id_vol, passager, nb_places, "
                  "prix_total, id_utilisateur, type_billet, ref_groupe, "
                  "avertissement_accepte_le) "
                  "SELECT :ref, id_vol, :p, :n, prix * :n2, :u, :t, :g, " + dateAvert +
                  " FROM VOL WHERE id_vol = :id");
        q.bindValue(":ref", ref);
        q.bindValue(":p", passager.trimmed());
        q.bindValue(":n", nb);
        q.bindValue(":n2", nb);
        q.bindValue(":u", idUtilisateur);
        q.bindValue(":t", typeBillet);
        q.bindValue(":g", refGroupe);
        q.bindValue(":id", ids[i]);
        if (!q.exec() || q.numRowsAffected() != 1) {
            qDebug() << "Itineraire : insertion" << q.lastError().text();
            m_db.rollback();
            return false;
        }
        int idRes = q.lastInsertId().toInt();
        q.finish();

        if (!attribuerSieges(idRes, ids[i], nb, *choix[i])) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}


bool DatabaseManager::simulerPerturbation(int idVol, const QString &type,
                                          int minutes, int &nbAlertes)
{
    nbAlertes = 0;
    if (type != "retard" && type != "annulation" && type != "normal")
        return false;
    if (type == "retard" && minutes <= 0)
        return false;
    if (!m_db.transaction())
        return false;

    QString statut = (type == "annulation") ? "annule"
                     : (type == "retard") ? "retarde" : "prevu";
    int retard = (type == "retard") ? minutes : 0;

    QSqlQuery q(m_db);
    q.prepare("UPDATE VOL SET statut_vol = :s, retard_minutes = :r WHERE id_vol = :id");
    q.bindValue(":s", statut);
    q.bindValue(":r", retard);
    q.bindValue(":id", idVol);
    if (!q.exec()) {
        qDebug() << "Simulation : mise a jour" << q.lastError().text();
        m_db.rollback();
        return false;
    }
    q.finish();

    q.prepare("SELECT numero_vol, ville_depart, ville_arrivee, date_arrivee "
              "FROM VOL WHERE id_vol = :id");
    q.bindValue(":id", idVol);
    if (!q.exec() || !q.next()) {
        m_db.rollback();
        return false;
    }
    QString numero = q.value(0).toString();
    QString trajet = q.value(1).toString() + " → " + q.value(2).toString();
    QDateTime arrivee = q.value(3).toDateTime();
    q.finish();

    if (type == "normal")                   // retour à la normale : pas d'alerte
        return m_db.commit();

    struct Res { int idUser; int idRes; QString ref; QString groupe; };
    QList<Res> liste;
    q.prepare("SELECT id_utilisateur, id_reservation, reference, IFNULL(ref_groupe, '') "
              "FROM RESERVATION WHERE id_vol = :id AND statut = 'confirmee' "
              "AND id_utilisateur IS NOT NULL");
    q.bindValue(":id", idVol);
    if (!q.exec()) {
        m_db.rollback();
        return false;
    }
    while (q.next()) {
        Res r;
        r.idUser = q.value(0).toInt();
        r.idRes = q.value(1).toInt();
        r.ref = q.value(2).toString();
        r.groupe = q.value(3).toString();
        liste.append(r);
    }
    q.finish();

    auto creerAlerte = [&](const Res &r, const QString &t, const QString &msg) {
        QSqlQuery a(m_db);
        a.prepare("INSERT INTO ALERTE (id_utilisateur, id_reservation, type, message) "
                  "VALUES (:u, :r, :t, :m)");
        a.bindValue(":u", r.idUser);
        a.bindValue(":r", r.idRes);
        a.bindValue(":t", t);
        a.bindValue(":m", msg);
        if (a.exec())
            ++nbAlertes;
        else
            qDebug() << "Erreur alerte :" << a.lastError().text();
    };

    for (const Res &r : liste) {
        if (type == "annulation")
            creerAlerte(r, "annulation",
                        "Votre vol " + numero + " (" + trajet + ") est ANNULÉ. "
                                                                "Réservation " + r.ref + ". Consultez les alternatives "
                                      "dans « Mon voyage ».");
        else
            creerAlerte(r, "retard",
                        "Votre vol " + numero + " (" + trajet + ") a "
                            + QString::number(minutes) + " minutes de retard. "
                                                         "Réservation " + r.ref + ".");

        // Réservation d'itinéraire : le vol suivant est-il menacé ?
        if (r.groupe.isEmpty())
            continue;
        QSqlQuery s(m_db);
        s.prepare("SELECT v2.numero_vol, v2.date_depart, IFNULL(a.mct_minutes, 45) "
                  "FROM RESERVATION r2 JOIN VOL v2 ON v2.id_vol = r2.id_vol "
                  "LEFT JOIN AEROPORT a ON a.code_iata = v2.aeroport_depart "
                  "WHERE r2.ref_groupe = :g AND r2.id_vol <> :id "
                  "AND v2.date_depart >= :arr "
                  "ORDER BY v2.date_depart LIMIT 1");
        s.bindValue(":g", r.groupe);
        s.bindValue(":id", idVol);
        s.bindValue(":arr", arrivee);
        if (!s.exec() || !s.next())
            continue;
        QString num2 = s.value(0).toString();
        QDateTime dep2 = s.value(1).toDateTime();
        int mct = s.value(2).toInt();

        if (type == "annulation") {
            creerAlerte(r, "correspondance",
                        "Le vol " + numero + " étant annulé, votre vol suivant "
                            + num2 + " ne peut plus être pris dans cet itinéraire. "
                                     "Contactez l'assistance.");
        } else {
            QDateTime arriveeRetard = arrivee.addSecs(qint64(minutes) * 60);
            qint64 marge = arriveeRetard.secsTo(dep2) / 60 - mct;
            if (marge < 0)
                creerAlerte(r, "correspondance",
                            "Correspondance MENACÉE : avec " + QString::number(minutes)
                                + " min de retard, vous risquez de manquer le vol "
                                + num2 + " (départ " + dep2.toString("dd/MM HH:mm") + ").");
        }
    }
    return m_db.commit();
}

int DatabaseManager::nombreAlertesNonLues(int idUtilisateur)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM ALERTE WHERE id_utilisateur = :u AND lue = 0");
    q.bindValue(":u", idUtilisateur);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}


// ===================== SIÈGES =====================

bool DatabaseManager::genererSieges(int idVol, int de, int a)
{
    if (de > a)
        return true;
    static const char lettres[] = "ABCDEF";
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO SIEGE (id_vol, numero, rangee, lettre, categorie, supplement) "
              "VALUES (:v, :n, :r, :l, :c, :s)");
    for (int i = de; i <= a; ++i) {
        int rangee = (i - 1) / 6 + 1;
        QString lettre(QChar::fromLatin1(lettres[(i - 1) % 6]));
        QString categorie = "standard";
        double supplement = 0.0;
        if (rangee <= 3) {
            categorie = "avant";
            supplement = 15.0;
        } else if (rangee == 10 || rangee == 11) {
            categorie = "sortie";
            supplement = 10.0;
        }
        q.bindValue(":v", idVol);
        q.bindValue(":n", QString::number(rangee) + lettre);
        q.bindValue(":r", rangee);
        q.bindValue(":l", lettre);
        q.bindValue(":c", categorie);
        q.bindValue(":s", supplement);
        if (!q.exec()) {
            qDebug() << "Erreur creation siege :" << q.lastError().text();
            return false;
        }
    }
    return true;
}

QList<int> DatabaseManager::siegesAutomatiques(int idVol, int nb)
{
    QList<int> libresGratuits;
    if (nb <= 0)
        return QList<int>();

    // Essai 0 : sièges gratuits seulement. Essai 1 : tous les sièges.
    for (int essai = 0; essai < 2; ++essai) {
        QString sql = "SELECT id_siege, rangee, lettre FROM SIEGE "
                      "WHERE id_vol = :v AND id_reservation IS NULL ";
        if (essai == 0)
            sql += "AND supplement = 0 ";
        sql += "ORDER BY rangee, lettre";

        QSqlQuery q(m_db);
        q.prepare(sql);
        q.bindValue(":v", idVol);
        if (!q.exec())
            return QList<int>();

        QList<int> tous, serie;
        int rangeePrec = -1;
        ushort lettrePrec = 0;
        while (q.next()) {
            int id = q.value(0).toInt();
            int rangee = q.value(1).toInt();
            ushort lettre = q.value(2).toString().at(0).unicode();
            tous.append(id);

            // Côte à côte : même rangée, lettres qui se suivent, pas à travers l'allée (C | D)
            bool voisin = (rangee == rangeePrec) && (lettre == lettrePrec + 1)
                          && lettrePrec != 'C';
            if (!voisin)
                serie.clear();
            serie.append(id);
            rangeePrec = rangee;
            lettrePrec = lettre;
            if (serie.size() == nb)
                return serie;
        }
        if (essai == 0)
            libresGratuits = tous;
        else if (libresGratuits.size() >= nb)
            return libresGratuits.mid(0, nb);   // pas côte à côte, mais gratuits
        else if (tous.size() >= nb)
            return tous.mid(0, nb);
    }
    return QList<int>();                        // pas assez de sièges libres
}

bool DatabaseManager::attribuerSieges(int idReservation, int idVol, int nb,
                                      const QList<int> &sieges, bool facturer)
{
    QList<int> choix = sieges;
    if (choix.isEmpty())
        choix = siegesAutomatiques(idVol, nb);
    if (choix.size() != nb) {
        qDebug() << "Sieges : nombre incorrect ou plus de places libres";
        return false;
    }

    QSqlQuery q(m_db);
    for (int idSiege : choix) {
        q.prepare("UPDATE SIEGE SET id_reservation = :r "
                  "WHERE id_siege = :s AND id_vol = :v AND id_reservation IS NULL");
        q.bindValue(":r", idReservation);
        q.bindValue(":s", idSiege);
        q.bindValue(":v", idVol);
        if (!q.exec() || q.numRowsAffected() != 1) {
            qDebug() << "Siege deja pris ou invalide :" << idSiege;
            return false;
        }
    }
    if (!facturer)
        return true;

    // Le supplément des sièges choisis s'ajoute au prix total
    q.prepare("UPDATE RESERVATION SET prix_total = prix_total + "
              "(SELECT IFNULL(SUM(supplement), 0) FROM SIEGE WHERE id_reservation = :r1) "
              "WHERE id_reservation = :r2");
    q.bindValue(":r1", idReservation);
    q.bindValue(":r2", idReservation);
    return q.exec();
}