#include "databasemanager.h"
#include "constantes.h"
#include "calculsvol.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTextStream>
#include <QUuid>
#include <QVariant>

QString DatabaseManager::cheminFichierConfig()
{
    // Je cherche config.ini dans le dossier courant, à côté de l'exécutable, ou à la racine du projet
    const QStringList candidats = {
        QDir::currentPath() + QStringLiteral("/config.ini"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config.ini")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../config.ini"))
    };
    for (const QString &chemin : candidats) {
        if (QFileInfo::exists(chemin))
            return QFileInfo(chemin).canonicalFilePath();
    }
    return QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
}

DatabaseManager::DatabaseManager()
{
    const QString nomConnexion = QStringLiteral("connexion_%1")
                                     .arg(QCoreApplication::applicationPid());
    if (QSqlDatabase::contains(nomConnexion)) {
        m_db = QSqlDatabase::database(nomConnexion);
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), nomConnexion);
    }

    QSettings cfg(cheminFichierConfig(), QSettings::IniFormat);
    QString hote = cfg.value(QStringLiteral("mysql/hote"),
                             QStringLiteral("localhost")).toString();
    int port = cfg.value(QStringLiteral("mysql/port"), 3306).toInt();
    QString base = cfg.value(QStringLiteral("mysql/base"),
                             QStringLiteral("reservation_vol_db")).toString();
    QString util = cfg.value(QStringLiteral("mysql/utilisateur"),
                             QStringLiteral("root")).toString();
    QString mdp = cfg.value(QStringLiteral("mysql/mot_de_passe"),
                            QStringLiteral("passwd")).toString();

    m_db.setHostName(hote);
    m_db.setPort(port);
    m_db.setDatabaseName(base);
    m_db.setUserName(util);
    m_db.setPassword(mdp);
}

DatabaseManager::~DatabaseManager()
{
    deconnecter();
}

bool DatabaseManager::connecter()
{
    if (m_db.open()) {
        assurerSchemaComplementaire();
        m_derniereErreur.clear();
        return true;
    }

    // Si mysql/mot_de_passe n'est pas défini dans config.ini et que "passwd" échoue, on teste un mot de passe vide (XAMPP/WAMP)
    QSettings cfg(cheminFichierConfig(), QSettings::IniFormat);
    if (!cfg.contains(QStringLiteral("mysql/mot_de_passe")) && m_db.password() == QStringLiteral("passwd")) {
        m_db.setPassword(QString());
        if (m_db.open()) {
            assurerSchemaComplementaire();
            m_derniereErreur.clear();
            return true;
        }
    }

    m_derniereErreur = m_db.lastError().text();
    return false;
}

void DatabaseManager::assurerSchemaComplementaire()
{
    QSqlQuery q(m_db);
    // Ajout automatique de est_bloque si la base avait déjà été importée avant cette mise à jour
    if (q.exec(QStringLiteral("SHOW COLUMNS FROM utilisateur LIKE 'est_bloque'")) && !q.next()) {
        q.exec(QStringLiteral(
            "ALTER TABLE utilisateur ADD COLUMN est_bloque TINYINT(1) NOT NULL DEFAULT 0"));
    }

    // Ajout automatique des colonnes de paiement TicketFly sur reservation si absentes
    if (q.exec(QStringLiteral("SHOW COLUMNS FROM reservation LIKE 'mode_paiement'")) && !q.next()) {
        q.exec(QStringLiteral(
            "ALTER TABLE reservation "
            "ADD COLUMN mode_paiement VARCHAR(40) NOT NULL DEFAULT '', "
            "ADD COLUMN contact_paiement VARCHAR(100) NOT NULL DEFAULT '', "
            "ADD COLUMN reference_paiement VARCHAR(80) NOT NULL DEFAULT ''"));
    }

    // Création automatique de la table de boîte de réception si elle n'existe pas encore
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS notification_message ("
        "  id_message INT NOT NULL AUTO_INCREMENT,"
        "  id_destinataire INT NOT NULL DEFAULT 0,"
        "  role_cible ENUM('client','admin') NOT NULL DEFAULT 'client',"
        "  id_expediteur INT NOT NULL DEFAULT 0,"
        "  nom_expediteur VARCHAR(100) NOT NULL,"
        "  email_expediteur VARCHAR(100) NOT NULL DEFAULT '',"
        "  categorie VARCHAR(40) NOT NULL DEFAULT 'Système',"
        "  sujet VARCHAR(160) NOT NULL,"
        "  contenu TEXT NOT NULL,"
        "  est_lu TINYINT(1) NOT NULL DEFAULT 0,"
        "  date_envoi DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (id_message),"
        "  KEY idx_notif_dest (id_destinataire, role_cible, est_lu)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"));
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

QString DatabaseManager::genererSelAleatoire() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
}

QString DatabaseManager::hacherMotDePasse(const QString &motDePasse, const QString &sel) const
{
    QByteArray entree = (sel + motDePasse).toUtf8();
    QByteArray hache = QCryptographicHash::hash(entree, QCryptographicHash::Sha256);
    return QString::fromLatin1(hache.toHex());
}

bool DatabaseManager::connecterUtilisateur(const QString &email,
                                           const QString &motDePasse,
                                           int &idUtilisateur,
                                           QString &role,
                                           QString &nomComplet,
                                           QString &emailNormalise,
                                           QString &erreur)
{
    QString mail = email.trimmed().toLower();
    if (mail.isEmpty() || motDePasse.isEmpty()) {
        erreur = QStringLiteral("Veuillez renseigner votre email et votre mot de passe.");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id_utilisateur, nom, prenom, mot_de_passe, role, est_bloque "
        "FROM utilisateur WHERE email = :e"));
    q.bindValue(QStringLiteral(":e"), mail);

    if (!q.exec()) {
        erreur = QStringLiteral("Erreur de lecture en base : ") + q.lastError().text();
        return false;
    }
    if (!q.next()) {
        erreur = QStringLiteral("Email ou mot de passe incorrect.");
        return false;
    }
    if (q.value(5).toInt() != 0) {
        erreur = QStringLiteral(
            "Ce compte est actuellement bloqué. Cliquez sur « Assistance » en haut de la fenêtre pour contacter un administrateur.");
        return false;
    }

    QString stocke = q.value(3).toString();
    QStringList parties = stocke.split(QLatin1Char('$'));
    if (parties.size() != 2 || hacherMotDePasse(motDePasse, parties[0]) != parties[1]) {
        erreur = QStringLiteral("Email ou mot de passe incorrect.");
        return false;
    }

    idUtilisateur = q.value(0).toInt();
    nomComplet = q.value(2).toString() + QStringLiteral(" ") + q.value(1).toString();
    role = q.value(4).toString();
    emailNormalise = mail;
    erreur.clear();
    return true;
}

static bool insererCompteUtilisateur(DatabaseManager *db,
                                     const QString &nom,
                                     const QString &prenom,
                                     const QString &email,
                                     const QString &motDePasse,
                                     const QString &roleCible,
                                     QString &erreur)
{
    if (nom.trimmed().size() < 2 || prenom.trimmed().size() < 2) {
        erreur = QStringLiteral("Le nom et le prénom doivent contenir au moins 2 lettres.");
        return false;
    }
    if (!CalculsVol::validerEmailStrict(email, erreur)) {
        return false;
    }
    if (motDePasse.size() < Constantes::LONGUEUR_MDP_MIN) {
        erreur = QStringLiteral("Le mot de passe doit contenir au moins %1 caractères.")
                     .arg(Constantes::LONGUEUR_MDP_MIN);
        return false;
    }

    QString mail = email.trimmed().toLower();
    QSqlQuery q(db->database());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM utilisateur WHERE email = :e"));
    q.bindValue(QStringLiteral(":e"), mail);
    if (!q.exec() || !q.next()) {
        erreur = QStringLiteral("Erreur lors de la vérification de l'email : ")
                 + q.lastError().text();
        return false;
    }
    if (q.value(0).toInt() > 0) {
        erreur = QStringLiteral("Cette adresse email est déjà utilisée par un autre compte.");
        return false;
    }
    q.finish();

    QString sel = db->genererSelAleatoire();
    QString motDePasseHache = sel + QLatin1Char('$') + db->hacherMotDePasse(motDePasse, sel);

    q.prepare(QStringLiteral(
        "INSERT INTO utilisateur (nom, prenom, email, mot_de_passe, role) "
        "VALUES (:n, :p, :e, :m, :r)"));
    q.bindValue(QStringLiteral(":n"), nom.trimmed().toUpper());
    q.bindValue(QStringLiteral(":p"), prenom.trimmed());
    q.bindValue(QStringLiteral(":e"), mail);
    q.bindValue(QStringLiteral(":m"), motDePasseHache);
    q.bindValue(QStringLiteral(":r"), roleCible);

    if (!q.exec()) {
        erreur = QStringLiteral("Impossible d'enregistrer le compte : ") + q.lastError().text();
        return false;
    }

    erreur.clear();
    return true;
}

bool DatabaseManager::inscrireClient(const QString &nom,
                                     const QString &prenom,
                                     const QString &email,
                                     const QString &motDePasse,
                                     QString &erreur)
{
    return insererCompteUtilisateur(
        this, nom, prenom, email, motDePasse, QStringLiteral("client"), erreur);
}

bool DatabaseManager::creerAdministrateur(const QString &roleAppelant,
                                          const QString &nom,
                                          const QString &prenom,
                                          const QString &email,
                                          const QString &motDePasse,
                                          QString &erreur)
{
    if (roleAppelant != QStringLiteral("super_admin")) {
        erreur = QStringLiteral("Seul le Super Administrateur peut créer un administrateur.");
        return false;
    }
    return insererCompteUtilisateur(
        this, nom, prenom, email, motDePasse, QStringLiteral("admin"), erreur);
}

bool DatabaseManager::modifierAdministrateur(const QString &roleAppelant,
                                             int idAdminCible,
                                             const QString &nom,
                                             const QString &prenom,
                                             const QString &email,
                                             const QString &nouveauMotDePasseOptionnel,
                                             QString &erreur)
{
    if (roleAppelant != QStringLiteral("super_admin")) {
        erreur = QStringLiteral("Seul le Super Administrateur peut modifier un administrateur.");
        return false;
    }
    if (nom.trimmed().size() < 2 || prenom.trimmed().size() < 2) {
        erreur = QStringLiteral("Le nom et le prénom doivent contenir au moins 2 lettres.");
        return false;
    }
    if (!CalculsVol::validerEmailStrict(email, erreur))
        return false;

    if (!nouveauMotDePasseOptionnel.isEmpty()
        && nouveauMotDePasseOptionnel.size() < Constantes::LONGUEUR_MDP_MIN) {
        erreur = QStringLiteral("Le nouveau mot de passe doit contenir au moins %1 caractères.")
                     .arg(Constantes::LONGUEUR_MDP_MIN);
        return false;
    }

    QString mail = email.trimmed().toLower();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM utilisateur WHERE email = :e AND id_utilisateur <> :id"));
    q.bindValue(QStringLiteral(":e"), mail);
    q.bindValue(QStringLiteral(":id"), idAdminCible);
    if (!q.exec() || !q.next() || q.value(0).toInt() > 0) {
        erreur = QStringLiteral("Cet email est déjà utilisé par un autre compte.");
        return false;
    }
    q.finish();

    if (nouveauMotDePasseOptionnel.isEmpty()) {
        q.prepare(QStringLiteral(
            "UPDATE utilisateur SET nom = :n, prenom = :p, email = :e "
            "WHERE id_utilisateur = :id AND role IN ('admin', 'super_admin')"));
    } else {
        QString sel = genererSelAleatoire();
        QString hache = sel + QLatin1Char('$') + hacherMotDePasse(nouveauMotDePasseOptionnel, sel);
        q.prepare(QStringLiteral(
            "UPDATE utilisateur SET nom = :n, prenom = :p, email = :e, mot_de_passe = :m "
            "WHERE id_utilisateur = :id AND role IN ('admin', 'super_admin')"));
        q.bindValue(QStringLiteral(":m"), hache);
    }
    q.bindValue(QStringLiteral(":n"), nom.trimmed().toUpper());
    q.bindValue(QStringLiteral(":p"), prenom.trimmed());
    q.bindValue(QStringLiteral(":e"), mail);
    q.bindValue(QStringLiteral(":id"), idAdminCible);

    if (!q.exec()) {
        erreur = QStringLiteral("Impossible de modifier l'administrateur : ") + q.lastError().text();
        return false;
    }
    erreur.clear();
    return true;
}

bool DatabaseManager::modifierClientParAdmin(const QString &roleAppelant,
                                             int idClientCible,
                                             const QString &nom,
                                             const QString &prenom,
                                             const QString &email,
                                             bool estBloque,
                                             const QString &nouveauMotDePasseOptionnel,
                                             QString &erreur)
{
    if (roleAppelant != QStringLiteral("admin") && roleAppelant != QStringLiteral("super_admin")) {
        erreur = QStringLiteral("Action réservée aux administrateurs.");
        return false;
    }
    if (nom.trimmed().size() < 2 || prenom.trimmed().size() < 2) {
        erreur = QStringLiteral("Le nom et le prénom doivent contenir au moins 2 lettres.");
        return false;
    }
    if (!CalculsVol::validerEmailStrict(email, erreur))
        return false;

    if (!nouveauMotDePasseOptionnel.isEmpty()
        && nouveauMotDePasseOptionnel.size() < Constantes::LONGUEUR_MDP_MIN) {
        erreur = QStringLiteral("Le mot de passe réinitialisé doit contenir au moins %1 caractères.")
                     .arg(Constantes::LONGUEUR_MDP_MIN);
        return false;
    }

    QString mail = email.trimmed().toLower();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM utilisateur WHERE email = :e AND id_utilisateur <> :id"));
    q.bindValue(QStringLiteral(":e"), mail);
    q.bindValue(QStringLiteral(":id"), idClientCible);
    if (!q.exec() || !q.next() || q.value(0).toInt() > 0) {
        erreur = QStringLiteral("Cet email est déjà utilisé par un autre compte.");
        return false;
    }
    q.finish();

    if (nouveauMotDePasseOptionnel.isEmpty()) {
        q.prepare(QStringLiteral(
            "UPDATE utilisateur SET nom = :n, prenom = :p, email = :e, est_bloque = :b "
            "WHERE id_utilisateur = :id AND role = 'client'"));
    } else {
        QString sel = genererSelAleatoire();
        QString hache = sel + QLatin1Char('$') + hacherMotDePasse(nouveauMotDePasseOptionnel, sel);
        q.prepare(QStringLiteral(
            "UPDATE utilisateur SET nom = :n, prenom = :p, email = :e, est_bloque = :b, mot_de_passe = :m "
            "WHERE id_utilisateur = :id AND role = 'client'"));
        q.bindValue(QStringLiteral(":m"), hache);
    }
    q.bindValue(QStringLiteral(":n"), nom.trimmed().toUpper());
    q.bindValue(QStringLiteral(":p"), prenom.trimmed());
    q.bindValue(QStringLiteral(":e"), mail);
    q.bindValue(QStringLiteral(":b"), estBloque ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), idClientCible);

    if (!q.exec()) {
        erreur = QStringLiteral("Impossible de mettre à jour le client : ") + q.lastError().text();
        return false;
    }

    QString detailNotif = QStringLiteral(
        "Les informations de votre compte client (%1 %2 — %3) ont été mises à jour par l'assistance.\nStatut du compte : %4.")
                              .arg(prenom.trimmed(),
                                   nom.trimmed().toUpper(),
                                   mail,
                                   estBloque ? QStringLiteral("Bloqué") : QStringLiteral("Actif"));
    if (!nouveauMotDePasseOptionnel.isEmpty())
        detailNotif += QStringLiteral("\nVotre mot de passe a également été réinitialisé.");

    QString ignoreErr;
    envoyerMessageBoite(idClientCible,
                        QStringLiteral("client"),
                        0,
                        QStringLiteral("Assistance Administrateur"),
                        QStringLiteral("support@billets-avion.mg"),
                        QStringLiteral("Assistance"),
                        QStringLiteral("Mise à jour de votre compte client"),
                        detailNotif,
                        ignoreErr);

    erreur.clear();
    return true;
}

bool DatabaseManager::supprimerClientParAdmin(const QString &roleAppelant,
                                              int idClientCible,
                                              QString &erreur)
{
    if (roleAppelant != QStringLiteral("admin") && roleAppelant != QStringLiteral("super_admin")) {
        erreur = QStringLiteral("Action réservée aux administrateurs.");
        return false;
    }

    QSqlQuery qVerif(m_db);
    qVerif.prepare(QStringLiteral("SELECT role FROM utilisateur WHERE id_utilisateur = :id"));
    qVerif.bindValue(QStringLiteral(":id"), idClientCible);
    if (!qVerif.exec() || !qVerif.next()) {
        erreur = QStringLiteral("Compte client introuvable.");
        return false;
    }
    if (qVerif.value(0).toString() != QStringLiteral("client")) {
        erreur = QStringLiteral("Seul un compte client peut être supprimé depuis cet onglet.");
        return false;
    }
    qVerif.finish();

    if (!m_db.transaction()) {
        erreur = QStringLiteral("Impossible de démarrer la transaction SQL.");
        return false;
    }

    // 1. Restituer les sièges des réservations confirmées de ce client
    QSqlQuery qPlaces(m_db);
    qPlaces.prepare(QStringLiteral(
        "SELECT rv.id_vol, r.nb_places "
        "FROM reservation r "
        "JOIN reservation_vol rv ON rv.id_reservation = r.id_reservation "
        "WHERE r.id_utilisateur = :u AND r.statut = 'confirmee'"));
    qPlaces.bindValue(QStringLiteral(":u"), idClientCible);
    if (qPlaces.exec()) {
        while (qPlaces.next()) {
            QSqlQuery qRestit(m_db);
            qRestit.prepare(QStringLiteral(
                "UPDATE vol SET places_disponibles = LEAST(capacite, places_disponibles + :nb) "
                "WHERE id_vol = :v"));
            qRestit.bindValue(QStringLiteral(":nb"), qPlaces.value(1).toInt());
            qRestit.bindValue(QStringLiteral(":v"), qPlaces.value(0).toInt());
            qRestit.exec();
        }
    }
    qPlaces.finish();

    // 2. Supprimer ses réservations (passager et reservation_vol suivent en CASCADE)
    QSqlQuery qDelRes(m_db);
    qDelRes.prepare(QStringLiteral("DELETE FROM reservation WHERE id_utilisateur = :u"));
    qDelRes.bindValue(QStringLiteral(":u"), idClientCible);
    if (!qDelRes.exec()) {
        m_db.rollback();
        erreur = QStringLiteral("Impossible de supprimer les réservations du client : ")
                 + qDelRes.lastError().text();
        return false;
    }

    // 3. Nettoyer ses messages de boîte de réception
    QSqlQuery qDelMsg(m_db);
    qDelMsg.prepare(QStringLiteral(
        "DELETE FROM notification_message "
        "WHERE (role_cible = 'client' AND id_destinataire = :u) OR id_expediteur = :u"));
    qDelMsg.bindValue(QStringLiteral(":u"), idClientCible);
    qDelMsg.exec();

    // 4. Supprimer le compte client lui-même
    QSqlQuery qDelUser(m_db);
    qDelUser.prepare(QStringLiteral(
        "DELETE FROM utilisateur WHERE id_utilisateur = :u AND role = 'client'"));
    qDelUser.bindValue(QStringLiteral(":u"), idClientCible);
    if (!qDelUser.exec() || qDelUser.numRowsAffected() != 1) {
        m_db.rollback();
        erreur = QStringLiteral("Impossible de supprimer le compte client.");
        return false;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        erreur = QStringLiteral("Impossible de valider la suppression SQL.");
        return false;
    }

    erreur.clear();
    return true;
}

bool DatabaseManager::supprimerAdministrateur(const QString &roleAppelant,
                                              int idAdminCible,
                                              QString &erreur)
{
    if (roleAppelant != QStringLiteral("super_admin")) {
        erreur = QStringLiteral("Seul le Super Administrateur peut supprimer un administrateur.");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT role FROM utilisateur WHERE id_utilisateur = :id"));
    q.bindValue(QStringLiteral(":id"), idAdminCible);
    if (!q.exec() || !q.next()) {
        erreur = QStringLiteral("Compte administrateur introuvable.");
        return false;
    }
    if (q.value(0).toString() != QStringLiteral("admin")) {
        erreur = QStringLiteral(
            "Impossible de supprimer un Super Administrateur ou un compte client ici.");
        return false;
    }
    q.finish();

    q.prepare(QStringLiteral(
        "DELETE FROM utilisateur WHERE id_utilisateur = :id AND role = 'admin'"));
    q.bindValue(QStringLiteral(":id"), idAdminCible);
    if (!q.exec() || q.numRowsAffected() != 1) {
        erreur = QStringLiteral("Suppression impossible (ce compte est peut-être lié à des données).");
        return false;
    }

    erreur.clear();
    return true;
}

QList<AeroportInfo> DatabaseManager::chargerAeroports()
{
    QList<AeroportInfo> liste;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT id_aeroport, code_iata, nom_aeroport, ville, pays, code_pays, "
            "latitude, longitude, est_hub "
            "FROM aeroport ORDER BY ville, code_iata"))) {
        m_derniereErreur = q.lastError().text();
        return liste;
    }

    while (q.next()) {
        AeroportInfo a;
        a.idAeroport = q.value(0).toInt();
        a.codeIata = q.value(1).toString();
        a.nomAeroport = q.value(2).toString();
        a.ville = q.value(3).toString();
        a.pays = q.value(4).toString();
        a.codePays = q.value(5).toString();
        a.latitude = q.value(6).toDouble();
        a.longitude = q.value(7).toDouble();
        a.estHub = (q.value(8).toInt() == 1);
        liste.append(a);
    }
    return liste;
}

QHash<QPair<int, int>, LiaisonReference> DatabaseManager::chargerLiaisons()
{
    QHash<QPair<int, int>, LiaisonReference> table;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT id_aeroport_depart, id_aeroport_arrivee, distance_km, duree_minutes, prix_base "
            "FROM liaison"))) {
        m_derniereErreur = q.lastError().text();
        return table;
    }

    while (q.next()) {
        LiaisonReference l;
        l.trouvee = true;
        l.distanceKm = q.value(2).toInt();
        l.dureeMinutes = q.value(3).toInt();
        l.prixBase = q.value(4).toDouble();
        table.insert(qMakePair(q.value(0).toInt(), q.value(1).toInt()), l);
    }
    return table;
}

QList<TronconVol> DatabaseManager::chargerVolsProgrammes()
{
    QList<TronconVol> vols;
    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "SELECT v.id_vol, v.numero_vol, v.compagnie, v.date_depart, v.date_arrivee, "
        "v.distance_km, v.duree_minutes, v.prix, v.places_disponibles, "
        "ad.id_aeroport, ad.code_iata, ad.nom_aeroport, ad.ville, ad.pays, ad.code_pays, ad.latitude, ad.longitude, ad.est_hub, "
        "aa.id_aeroport, aa.code_iata, aa.nom_aeroport, aa.ville, aa.pays, aa.code_pays, aa.latitude, aa.longitude, aa.est_hub "
        "FROM vol v "
        "JOIN aeroport ad ON ad.id_aeroport = v.id_aeroport_depart "
        "JOIN aeroport aa ON aa.id_aeroport = v.id_aeroport_arrivee "
        "WHERE v.statut_vol = 'programme' AND v.date_depart > NOW() "
        "ORDER BY v.date_depart ASC");

    if (!q.exec(sql)) {
        m_derniereErreur = q.lastError().text();
        return vols;
    }

    while (q.next()) {
        TronconVol t;
        t.idVolExistant = q.value(0).toInt();
        t.numeroVol = q.value(1).toString();
        t.compagnie = q.value(2).toString();
        t.dateDepart = q.value(3).toDateTime();
        t.dateArrivee = q.value(4).toDateTime();
        t.distanceKm = q.value(5).toInt();
        t.dureeMinutes = q.value(6).toInt();
        t.prixUnitaire = q.value(7).toDouble();
        t.placesDisponibles = q.value(8).toInt();

        t.depart.idAeroport = q.value(9).toInt();
        t.depart.codeIata = q.value(10).toString();
        t.depart.nomAeroport = q.value(11).toString();
        t.depart.ville = q.value(12).toString();
        t.depart.pays = q.value(13).toString();
        t.depart.codePays = q.value(14).toString();
        t.depart.latitude = q.value(15).toDouble();
        t.depart.longitude = q.value(16).toDouble();
        t.depart.estHub = (q.value(17).toInt() == 1);

        t.arrivee.idAeroport = q.value(18).toInt();
        t.arrivee.codeIata = q.value(19).toString();
        t.arrivee.nomAeroport = q.value(20).toString();
        t.arrivee.ville = q.value(21).toString();
        t.arrivee.pays = q.value(22).toString();
        t.arrivee.codePays = q.value(23).toString();
        t.arrivee.latitude = q.value(24).toDouble();
        t.arrivee.longitude = q.value(25).toDouble();
        t.arrivee.estHub = (q.value(26).toInt() == 1);

        vols.append(t);
    }
    return vols;
}

QString DatabaseManager::genererNumeroVolUnique(const QString &codePaysDepart)
{
    QString prefixe = (codePaysDepart == QStringLiteral("MG"))
                          ? QStringLiteral("MD")
                          : (codePaysDepart == QStringLiteral("FR") ? QStringLiteral("AF")
                                                                    : QStringLiteral("AI"));

    QSqlQuery q(m_db);
    for (int essai = 0; essai < 50; ++essai) {
        int codeNum = QRandomGenerator::global()->bounded(1000, 9999);
        QString candidat = QStringLiteral("%1-%2").arg(prefixe).arg(codeNum);

        q.prepare(QStringLiteral("SELECT COUNT(*) FROM vol WHERE numero_vol = :n"));
        q.bindValue(QStringLiteral(":n"), candidat);
        if (q.exec() && q.next() && q.value(0).toInt() == 0) {
            return candidat;
        }
    }
    return prefixe + QLatin1Char('-')
           + QUuid::createUuid().toString(QUuid::WithoutBraces).left(4).toUpper();
}

bool DatabaseManager::ajouterVolManuel(const QString &compagnie,
                                       int idAeroportDepart,
                                       int idAeroportArrivee,
                                       const QDateTime &dateDepart,
                                       int capacite,
                                       double prixForce,
                                       QString &numeroCree,
                                       QString &erreur)
{
    if (idAeroportDepart <= 0 || idAeroportArrivee <= 0 || idAeroportDepart == idAeroportArrivee) {
        erreur = QStringLiteral("Les aéroports de départ et d'arrivée doivent être différents.");
        return false;
    }
    if (dateDepart <= QDateTime::currentDateTime()) {
        erreur = QStringLiteral("Impossible de programmer un vol dans le passé.");
        return false;
    }
    if (capacite < Constantes::NB_PASSAGERS_MIN) {
        erreur = QStringLiteral("La capacité du vol doit être supérieure à zéro.");
        return false;
    }

    QList<AeroportInfo> aeroports = chargerAeroports();
    AeroportInfo dep, arr;
    for (const AeroportInfo &a : aeroports) {
        if (a.idAeroport == idAeroportDepart)
            dep = a;
        if (a.idAeroport == idAeroportArrivee)
            arr = a;
    }
    if (!dep.estValide() || !arr.estValide()) {
        erreur = QStringLiteral("Aéroport introuvable en base.");
        return false;
    }

    LiaisonReference ref = CalculsVol::calculerReferenceTroncon(dep, arr, chargerLiaisons());
    double prixFinal = (prixForce > 0.0) ? prixForce : ref.prixBase;
    QDateTime dateArrivee = dateDepart.addSecs(
        static_cast<qint64>(ref.dureeMinutes) * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
    numeroCree = genererNumeroVolUnique(dep.codePays);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO vol (numero_vol, compagnie, id_aeroport_depart, id_aeroport_arrivee, "
        "date_depart, date_arrivee, distance_km, duree_minutes, prix, capacite, places_disponibles, statut_vol) "
        "VALUES (:num, :comp, :dep, :arr, :dd, :da, :dist, :dur, :prix, :cap, :places, 'programme')"));
    q.bindValue(QStringLiteral(":num"), numeroCree);
    q.bindValue(QStringLiteral(":comp"),
                compagnie.trimmed().isEmpty() ? QStringLiteral("Madagascar Airlines")
                                              : compagnie.trimmed());
    q.bindValue(QStringLiteral(":dep"), idAeroportDepart);
    q.bindValue(QStringLiteral(":arr"), idAeroportArrivee);
    q.bindValue(QStringLiteral(":dd"), dateDepart.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.bindValue(QStringLiteral(":da"), dateArrivee.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.bindValue(QStringLiteral(":dist"), ref.distanceKm);
    q.bindValue(QStringLiteral(":dur"), ref.dureeMinutes);
    q.bindValue(QStringLiteral(":prix"), prixFinal);
    q.bindValue(QStringLiteral(":cap"), capacite);
    q.bindValue(QStringLiteral(":places"), capacite);

    if (!q.exec()) {
        erreur = QStringLiteral("Erreur d'ajout du vol : ") + q.lastError().text();
        return false;
    }

    erreur.clear();
    return true;
}

bool DatabaseManager::modifierVolManuel(int idVol,
                                        const QString &compagnie,
                                        int idAeroportDepart,
                                        int idAeroportArrivee,
                                        const QDateTime &dateDepart,
                                        int capacite,
                                        double prixForce,
                                        bool notifierClients,
                                        QStringList &referencesReservationsImpactees,
                                        QString &erreur)
{
    referencesReservationsImpactees.clear();

    if (idVol <= 0) {
        erreur = QStringLiteral("Sélectionnez un vol valide à modifier.");
        return false;
    }
    if (idAeroportDepart == idAeroportArrivee) {
        erreur = QStringLiteral("L'aéroport de départ et l'aéroport d'arrivée doivent être différents.");
        return false;
    }
    if (dateDepart <= QDateTime::currentDateTime()) {
        erreur = QStringLiteral("Impossible de déplacer un vol dans le passé.");
        return false;
    }

    QSqlQuery qLectureVol(m_db);
    qLectureVol.prepare(QStringLiteral(
        "SELECT numero_vol, capacite, places_disponibles FROM vol WHERE id_vol = :id"));
    qLectureVol.bindValue(QStringLiteral(":id"), idVol);
    if (!qLectureVol.exec() || !qLectureVol.next()) {
        erreur = QStringLiteral("Vol introuvable en base.");
        return false;
    }
    const QString numeroVol = qLectureVol.value(0).toString();
    const int ancienneCap = qLectureVol.value(1).toInt();
    const int anciennesDispo = qLectureVol.value(2).toInt();
    const int placesDejaPrises = qMax(0, ancienneCap - anciennesDispo);
    qLectureVol.finish();

    if (capacite < qMax(Constantes::NB_PASSAGERS_MIN, placesDejaPrises)) {
        erreur = QStringLiteral("Capacité insuffisante : %1 place(s) sont déjà confirmées sur ce vol.")
                     .arg(placesDejaPrises);
        return false;
    }

    QList<AeroportInfo> aeroports = chargerAeroports();
    AeroportInfo dep, arr;
    for (const AeroportInfo &a : aeroports) {
        if (a.idAeroport == idAeroportDepart) dep = a;
        if (a.idAeroport == idAeroportArrivee) arr = a;
    }
    if (!dep.estValide() || !arr.estValide()) {
        erreur = QStringLiteral("Aéroport introuvable.");
        return false;
    }

    LiaisonReference ref = CalculsVol::calculerReferenceTroncon(dep, arr, chargerLiaisons());
    double prixFinal = (prixForce > 0.0) ? prixForce : ref.prixBase;
    QDateTime dateArrivee = dateDepart.addSecs(
        static_cast<qint64>(ref.dureeMinutes) * static_cast<qint64>(Constantes::MINUTES_PAR_HEURE));
    int nouvellesDispo = capacite - placesDejaPrises;

    QSqlQuery qMaj(m_db);
    qMaj.prepare(QStringLiteral(
        "UPDATE vol SET compagnie = :comp, id_aeroport_depart = :dep, id_aeroport_arrivee = :arr, "
        "date_depart = :dd, date_arrivee = :da, distance_km = :dist, duree_minutes = :dur, "
        "prix = :prix, capacite = :cap, places_disponibles = :dispo WHERE id_vol = :id"));
    qMaj.bindValue(QStringLiteral(":comp"),
                   compagnie.trimmed().isEmpty() ? QStringLiteral("Madagascar Airlines")
                                                 : compagnie.trimmed());
    qMaj.bindValue(QStringLiteral(":dep"), idAeroportDepart);
    qMaj.bindValue(QStringLiteral(":arr"), idAeroportArrivee);
    qMaj.bindValue(QStringLiteral(":dd"), dateDepart.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    qMaj.bindValue(QStringLiteral(":da"), dateArrivee.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    qMaj.bindValue(QStringLiteral(":dist"), ref.distanceKm);
    qMaj.bindValue(QStringLiteral(":dur"), ref.dureeMinutes);
    qMaj.bindValue(QStringLiteral(":prix"), prixFinal);
    qMaj.bindValue(QStringLiteral(":cap"), capacite);
    qMaj.bindValue(QStringLiteral(":dispo"), nouvellesDispo);
    qMaj.bindValue(QStringLiteral(":id"), idVol);

    if (!qMaj.exec()) {
        erreur = QStringLiteral("Erreur lors de la modification du vol : ") + qMaj.lastError().text();
        return false;
    }

    // Recherche des réservations actives rattachées à ce vol
    QSqlQuery qRes(m_db);
    qRes.prepare(QStringLiteral(
        "SELECT DISTINCT r.reference, r.id_utilisateur "
        "FROM reservation r "
        "JOIN reservation_vol rv ON rv.id_reservation = r.id_reservation "
        "WHERE rv.id_vol = :v AND r.statut <> 'annulee'"));
    qRes.bindValue(QStringLiteral(":v"), idVol);
    if (qRes.exec()) {
        while (qRes.next()) {
            QString refRes = qRes.value(0).toString();
            int idClient = qRes.value(1).toInt();
            referencesReservationsImpactees.append(refRes);

            if (notifierClients && idClient > 0) {
                QString ignoreErr;
                envoyerMessageBoite(
                    idClient, QStringLiteral("client"), 0,
                    QStringLiteral("TicketFly — Opérations Vols"),
                    QStringLiteral("noreply@billets-avion.mg"),
                    QStringLiteral("Vol modifié"),
                    QStringLiteral("Vol %1 modifié (Réservation %2)").arg(numeroVol, refRes),
                    QStringLiteral("Votre vol %1 sur le dossier %2 a été mis à jour :\n"
                                   "Nouveau trajet : %3 (%4) → %5 (%6)\n"
                                   "Nouveau départ : %7  |  Arrivée : %8\n"
                                   "Votre nouveau billet PDF mis à jour est disponible dans « Mes réservations ».")
                        .arg(numeroVol, refRes, dep.ville, dep.codeIata, arr.ville, arr.codeIata,
                             dateDepart.toString(QStringLiteral("dd/MM/yyyy HH:mm")),
                             dateArrivee.toString(QStringLiteral("dd/MM/yyyy HH:mm"))),
                    ignoreErr);
            }
        }
    }

    erreur.clear();
    return true;
}

bool DatabaseManager::supprimerVol(int idVol, QString &erreur)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM reservation_vol WHERE id_vol = :id"));
    q.bindValue(QStringLiteral(":id"), idVol);
    if (!q.exec() || !q.next()) {
        erreur = QStringLiteral("Erreur de vérification : ") + q.lastError().text();
        return false;
    }
    if (q.value(0).toInt() > 0) {
        erreur = QStringLiteral(
            "Ce vol est rattaché à une ou plusieurs réservations et ne peut pas être supprimé.");
        return false;
    }
    q.finish();

    q.prepare(QStringLiteral("DELETE FROM vol WHERE id_vol = :id"));
    q.bindValue(QStringLiteral(":id"), idVol);
    if (!q.exec() || q.numRowsAffected() != 1) {
        erreur = QStringLiteral("Impossible de supprimer ce vol.");
        return false;
    }

    erreur.clear();
    return true;
}

QSet<QString> DatabaseManager::chargerSiegesOccupesVol(int idVol)
{
    QSet<QString> occupes;
    if (idVol <= 0)
        return occupes;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT p.siege FROM passager p "
        "JOIN reservation_vol rv ON rv.id_reservation = p.id_reservation "
        "JOIN reservation r ON r.id_reservation = p.id_reservation "
        "WHERE rv.id_vol = :v AND r.statut <> 'annulee'"));
    q.bindValue(QStringLiteral(":v"), idVol);
    if (q.exec()) {
        while (q.next()) {
            // Extrait le code siège court (ex: "2A" depuis "2A" ou "2A (Classe A)")
            QString brut = q.value(0).toString().trimmed().section(QLatin1Char(' '), 0, 0).toUpper();
            if (!brut.isEmpty())
                occupes.insert(brut);
        }
    }
    return occupes;
}

bool DatabaseManager::creerReservation(int idUtilisateur,
                                       const ItinerairePropose &itineraire,
                                       const QList<PassagerInfo> &passagers,
                                       QString &referenceCreee,
                                       QStringList &numerosVolsFinaux,
                                       QString &erreur,
                                       double supplementSiegesTotal,
                                       const InfosPaiement &paiement)
{
    if (idUtilisateur <= 0) {
        erreur = QStringLiteral("Vous devez être connecté pour réserver.");
        return false;
    }
    if (itineraire.troncons.isEmpty()
        || itineraire.nbEscales() > Constantes::NB_ESCALES_MAX) {
        erreur = QStringLiteral("Itinéraire invalide (maximum 2 escales autorisées).");
        return false;
    }
    if (passagers.isEmpty()) {
        erreur = QStringLiteral("Veuillez renseigner au moins un passager.");
        return false;
    }
    if (itineraire.dateDepartInitiale() <= QDateTime::currentDateTime()) {
        erreur = QStringLiteral("Réservation refusée : la date ou l'heure de départ est dans le passé.");
        return false;
    }

    // Je revérifie l'absence totale de chevauchement entre les escales avant toute écriture SQL
    for (int i = 0; i < itineraire.troncons.size() - 1; ++i) {
        if (itineraire.troncons[i].dateArrivee >= itineraire.troncons[i + 1].dateDepart) {
            erreur = QStringLiteral("Incohérence horaire détectée entre deux escales.");
            return false;
        }
    }

    const int nbPlaces = passagers.size();
    const double prixTotal = qRound((itineraire.prixParPassager * nbPlaces + supplementSiegesTotal)
                                    * Constantes::FACTEUR_ARRONDI_CENTIMES)
                             / Constantes::FACTEUR_ARRONDI_CENTIMES;

    if (!m_db.transaction()) {
        erreur = QStringLiteral("Impossible de démarrer la transaction SQL.");
        return false;
    }

    QList<int> idsVolsFinaux;
    numerosVolsFinaux.clear();

    for (const TronconVol &t : itineraire.troncons) {
        int idVol = t.idVolExistant;
        QString numVol = t.numeroVol;

        if (idVol > 0) {
            QSqlQuery qVerif(m_db);
            qVerif.prepare(QStringLiteral(
                "SELECT places_disponibles, numero_vol, date_depart "
                "FROM vol WHERE id_vol = :id AND statut_vol = 'programme' FOR UPDATE"));
            qVerif.bindValue(QStringLiteral(":id"), idVol);
            if (!qVerif.exec() || !qVerif.next()) {
                m_db.rollback();
                erreur = QStringLiteral("L'un des vols sélectionnés n'est plus disponible.");
                return false;
            }
            if (qVerif.value(2).toDateTime() <= QDateTime::currentDateTime()) {
                m_db.rollback();
                erreur = QStringLiteral("Ce vol est déjà parti.");
                return false;
            }
            if (qVerif.value(0).toInt() < nbPlaces) {
                m_db.rollback();
                erreur = QStringLiteral("Capacité insuffisante sur le vol %1.")
                             .arg(qVerif.value(1).toString());
                return false;
            }
            numVol = qVerif.value(1).toString();
        } else {
            // Je vérifie d'abord si un vol identique a déjà été créé à cet horaire exact
            QSqlQuery qExist(m_db);
            qExist.prepare(QStringLiteral(
                "SELECT id_vol, numero_vol, places_disponibles FROM vol "
                "WHERE id_aeroport_depart = :dep AND id_aeroport_arrivee = :arr "
                "AND date_depart = :dd AND statut_vol = 'programme' FOR UPDATE"));
            qExist.bindValue(QStringLiteral(":dep"), t.depart.idAeroport);
            qExist.bindValue(QStringLiteral(":arr"), t.arrivee.idAeroport);
            qExist.bindValue(QStringLiteral(":dd"),
                             t.dateDepart.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

            if (qExist.exec() && qExist.next()) {
                if (qExist.value(2).toInt() < nbPlaces) {
                    m_db.rollback();
                    erreur = QStringLiteral("Plus assez de places sur le vol %1.")
                                 .arg(qExist.value(1).toString());
                    return false;
                }
                idVol = qExist.value(0).toInt();
                numVol = qExist.value(1).toString();
            } else {
                // Génération automatique du numéro de vol lors de la réservation
                numVol = genererNumeroVolUnique(t.depart.codePays);
                QSqlQuery qInsVol(m_db);
                qInsVol.prepare(QStringLiteral(
                    "INSERT INTO vol (numero_vol, compagnie, id_aeroport_depart, id_aeroport_arrivee, "
                    "date_depart, date_arrivee, distance_km, duree_minutes, prix, capacite, places_disponibles, statut_vol) "
                    "VALUES (:num, :comp, :dep, :arr, :dd, :da, :dist, :dur, :prix, :cap, :places, 'programme')"));
                qInsVol.bindValue(QStringLiteral(":num"), numVol);
                qInsVol.bindValue(QStringLiteral(":comp"), t.compagnie);
                qInsVol.bindValue(QStringLiteral(":dep"), t.depart.idAeroport);
                qInsVol.bindValue(QStringLiteral(":arr"), t.arrivee.idAeroport);
                qInsVol.bindValue(QStringLiteral(":dd"),
                                  t.dateDepart.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
                qInsVol.bindValue(QStringLiteral(":da"),
                                  t.dateArrivee.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
                qInsVol.bindValue(QStringLiteral(":dist"), t.distanceKm);
                qInsVol.bindValue(QStringLiteral(":dur"), t.dureeMinutes);
                qInsVol.bindValue(QStringLiteral(":prix"), t.prixUnitaire);
                qInsVol.bindValue(QStringLiteral(":cap"), Constantes::CAPACITE_CABINE_DEFAUT);
                qInsVol.bindValue(QStringLiteral(":places"), Constantes::CAPACITE_CABINE_DEFAUT);

                if (!qInsVol.exec()) {
                    m_db.rollback();
                    erreur = QStringLiteral("Erreur lors de la création automatique du vol : ")
                             + qInsVol.lastError().text();
                    return false;
                }
                idVol = qInsVol.lastInsertId().toInt();
            }
        }

        idsVolsFinaux.append(idVol);
        numerosVolsFinaux.append(numVol);
    }

    referenceCreee = QStringLiteral("RES-")
                     + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();

    QSqlQuery qRes(m_db);
    qRes.prepare(QStringLiteral(
        "INSERT INTO reservation (reference, id_utilisateur, nb_places, prix_total, statut, nb_escales, "
        "mode_paiement, contact_paiement, reference_paiement) "
        "VALUES (:ref, :u, :nb, :prix, 'en_attente', :esc, :mp, :cp, :rp)"));
    qRes.bindValue(QStringLiteral(":ref"), referenceCreee);
    qRes.bindValue(QStringLiteral(":u"), idUtilisateur);
    qRes.bindValue(QStringLiteral(":nb"), nbPlaces);
    qRes.bindValue(QStringLiteral(":prix"), prixTotal);
    qRes.bindValue(QStringLiteral(":esc"), itineraire.nbEscales());
    qRes.bindValue(QStringLiteral(":mp"), paiement.modePaiement.trimmed());
    qRes.bindValue(QStringLiteral(":cp"), paiement.contactPaiement.trimmed());
    qRes.bindValue(QStringLiteral(":rp"), paiement.referencePaiement.trimmed());

    if (!qRes.exec()) {
        m_db.rollback();
        erreur = QStringLiteral("Erreur d'enregistrement de la réservation : ")
                 + qRes.lastError().text();
        return false;
    }

    const int idReservation = qRes.lastInsertId().toInt();

    for (int i = 0; i < idsVolsFinaux.size(); ++i) {
        QSqlQuery qLien(m_db);
        qLien.prepare(QStringLiteral(
            "INSERT INTO reservation_vol (id_reservation, ordre_troncon, id_vol) "
            "VALUES (:r, :ordre, :v)"));
        qLien.bindValue(QStringLiteral(":r"), idReservation);
        qLien.bindValue(QStringLiteral(":ordre"), i + 1);
        qLien.bindValue(QStringLiteral(":v"), idsVolsFinaux[i]);
        if (!qLien.exec()) {
            m_db.rollback();
            erreur = QStringLiteral("Erreur de liaison des tronçons : ") + qLien.lastError().text();
            return false;
        }
    }

    // Attribution des sièges : utilise le siège choisi sur le plan d'avion (sinon attribution auto)
    QSet<QString> dejaPris = chargerSiegesOccupesVol(idsVolsFinaux.first());
    static const char lettresCabine[] = "ABCDEF";
    int curseurAuto = 0;

    for (int i = 0; i < passagers.size(); ++i) {
        QString codeSiege = passagers[i].siege.trimmed();
        QString codeCourt = codeSiege.section(QLatin1Char(' '), 0, 0).toUpper();

        if (!codeCourt.isEmpty()) {
            if (dejaPris.contains(codeCourt)) {
                m_db.rollback();
                erreur = QStringLiteral("Le siège %1 vient d'être réservé par un autre voyageur.")
                             .arg(codeCourt);
                return false;
            }
            dejaPris.insert(codeCourt);
        } else {
            while (true) {
                int rangee = (curseurAuto / Constantes::SIEGES_PAR_RANGEE)
                             + Constantes::RANGEE_MAX_CLASSE_A + 1;
                char lettre = lettresCabine[curseurAuto % Constantes::SIEGES_PAR_RANGEE];
                ++curseurAuto;
                QString candidat = QStringLiteral("%1%2").arg(rangee).arg(QLatin1Char(lettre));
                if (!dejaPris.contains(candidat)) {
                    codeSiege = candidat;
                    dejaPris.insert(candidat);
                    break;
                }
            }
        }

        QSqlQuery qPass(m_db);
        qPass.prepare(QStringLiteral(
            "INSERT INTO passager (id_reservation, nom, prenom, date_naissance, numero_passeport, siege) "
            "VALUES (:r, :nom, :pre, :dn, :num, :siege)"));
        qPass.bindValue(QStringLiteral(":r"), idReservation);
        qPass.bindValue(QStringLiteral(":nom"), passagers[i].nom.trimmed().toUpper());
        qPass.bindValue(QStringLiteral(":pre"), passagers[i].prenom.trimmed());
        qPass.bindValue(QStringLiteral(":dn"),
                        passagers[i].dateNaissance.toString(QStringLiteral("yyyy-MM-dd")));
        qPass.bindValue(QStringLiteral(":num"), passagers[i].numeroPasseport.trimmed().toUpper());
        qPass.bindValue(QStringLiteral(":siege"), codeSiege);

        if (!qPass.exec()) {
            m_db.rollback();
            erreur = QStringLiteral("Erreur d'enregistrement passager : ")
                     + qPass.lastError().text();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_db.rollback();
        erreur = QStringLiteral("Impossible de valider la transaction SQL.");
        return false;
    }

    const QString lignePaiement = paiement.referencePaiement.trimmed().isEmpty()
        ? QStringLiteral("Paiement non renseigné")
        : QStringLiteral("%1 — %2 — Réf : %3")
              .arg(paiement.modePaiement, paiement.contactPaiement, paiement.referencePaiement);

    // Notifications automatiques dans la boîte de réception (Client + Admins avec référence de paiement)
    QString ignoreErr;
    envoyerMessageBoite(
        idUtilisateur,
        QStringLiteral("client"),
        0,
        QStringLiteral("TicketFly Réservations"),
        QStringLiteral("noreply@billets-avion.mg"),
        QStringLiteral("Réservation"),
        QStringLiteral("Réservation %1 reçue (En attente)").arg(referenceCreee),
        QStringLiteral("Votre demande %1 pour %2 (%3 passager(s)) a bien été enregistrée.\n"
                       "Paiement déclaré : %4.\n"
                       "Un administrateur vérifie votre référence avant d'émettre votre billet PDF.")
            .arg(referenceCreee, itineraire.resumeTrajet(),
                 QString::number(nbPlaces), lignePaiement),
        ignoreErr);

    envoyerMessageBoite(
        0,
        QStringLiteral("admin"),
        idUtilisateur,
        QStringLiteral("Client #%1").arg(idUtilisateur),
        QString(),
        QStringLiteral("Paiement"),
        QStringLiteral("Nouvelle réservation %1 — %2").arg(referenceCreee, lignePaiement),
        QStringLiteral("Dossier : %1  |  Trajet : %2 (%3 passager(s))\n"
                       "Montant base EUR : %4 €\n"
                       "Paiement TicketFly : %5\n"
                       "Vérifiez la référence puis confirmez dans Administration → Réservations.")
            .arg(referenceCreee,
                 itineraire.resumeTrajet(),
                 QString::number(nbPlaces),
                 QString::number(prixTotal, 'f', 2),
                 lignePaiement),
        ignoreErr);

    erreur.clear();
    return true;
}

bool DatabaseManager::changerStatutReservation(const QString &reference,
                                               const QString &nouveauStatut,
                                               QString &erreur)
{
    if (nouveauStatut != QStringLiteral("en_attente")
        && nouveauStatut != QStringLiteral("confirmee")
        && nouveauStatut != QStringLiteral("annulee")) {
        erreur = QStringLiteral("Statut demandé invalide.");
        return false;
    }

    if (!m_db.transaction()) {
        erreur = QStringLiteral("Impossible d'ouvrir la transaction SQL.");
        return false;
    }

    QSqlQuery qRes(m_db);
    qRes.prepare(QStringLiteral(
        "SELECT id_reservation, nb_places, statut, id_utilisateur FROM reservation "
        "WHERE reference = :ref FOR UPDATE"));
    qRes.bindValue(QStringLiteral(":ref"), reference);
    if (!qRes.exec() || !qRes.next()) {
        m_db.rollback();
        erreur = QStringLiteral("Réservation introuvable.");
        return false;
    }

    int idRes = qRes.value(0).toInt();
    int nbPlaces = qRes.value(1).toInt();
    QString ancienStatut = qRes.value(2).toString();
    int idClientTitulaire = qRes.value(3).toInt();
    qRes.finish();

    if (ancienStatut == nouveauStatut) {
        m_db.rollback();
        erreur.clear();
        return true;
    }

    QList<int> idsVols;
    QSqlQuery qVols(m_db);
    qVols.prepare(QStringLiteral(
        "SELECT id_vol FROM reservation_vol WHERE id_reservation = :r ORDER BY ordre_troncon"));
    qVols.bindValue(QStringLiteral(":r"), idRes);
    if (!qVols.exec()) {
        m_db.rollback();
        erreur = QStringLiteral("Impossible de lire les vols associés.");
        return false;
    }
    while (qVols.next()) {
        idsVols.append(qVols.value(0).toInt());
    }
    qVols.finish();

    // Si l'admin confirme la réservation, je vérifie la capacité restante de chaque vol et je débite les places
    if (ancienStatut != QStringLiteral("confirmee")
        && nouveauStatut == QStringLiteral("confirmee")) {
        for (int idVol : idsVols) {
            QSqlQuery qCheck(m_db);
            qCheck.prepare(QStringLiteral(
                "SELECT numero_vol, places_disponibles, statut_vol "
                "FROM vol WHERE id_vol = :v FOR UPDATE"));
            qCheck.bindValue(QStringLiteral(":v"), idVol);
            if (!qCheck.exec() || !qCheck.next()) {
                m_db.rollback();
                erreur = QStringLiteral("Vol introuvable lors de la vérification de capacité.");
                return false;
            }
            QString numVol = qCheck.value(0).toString();
            int dispo = qCheck.value(1).toInt();
            QString stVol = qCheck.value(2).toString();
            qCheck.finish();

            if (stVol != QStringLiteral("programme")) {
                m_db.rollback();
                erreur = QStringLiteral("Le vol %1 est annulé.").arg(numVol);
                return false;
            }
            if (dispo < nbPlaces) {
                m_db.rollback();
                erreur = QStringLiteral(
                    "Capacité insuffisante sur le vol %1 : %2 place(s) disponible(s) pour %3 demandée(s).")
                             .arg(numVol)
                             .arg(dispo)
                             .arg(nbPlaces);
                return false;
            }

            QSqlQuery qDeb(m_db);
            qDeb.prepare(QStringLiteral(
                "UPDATE vol SET places_disponibles = places_disponibles - :nb WHERE id_vol = :v"));
            qDeb.bindValue(QStringLiteral(":nb"), nbPlaces);
            qDeb.bindValue(QStringLiteral(":v"), idVol);
            if (!qDeb.exec()) {
                m_db.rollback();
                erreur = QStringLiteral("Erreur lors de la mise à jour des places.");
                return false;
            }
        }
    } else if (ancienStatut == QStringLiteral("confirmee")
               && nouveauStatut != QStringLiteral("confirmee")) {
        // Si une réservation déjà confirmée repasse en attente ou est annulée, on libère les places
        for (int idVol : idsVols) {
            QSqlQuery qCred(m_db);
            qCred.prepare(QStringLiteral(
                "UPDATE vol SET places_disponibles = LEAST(capacite, places_disponibles + :nb) "
                "WHERE id_vol = :v"));
            qCred.bindValue(QStringLiteral(":nb"), nbPlaces);
            qCred.bindValue(QStringLiteral(":v"), idVol);
            if (!qCred.exec()) {
                m_db.rollback();
                erreur = QStringLiteral("Erreur lors de la restitution des places.");
                return false;
            }
        }
    }

    QSqlQuery qMaj(m_db);
    qMaj.prepare(QStringLiteral(
        "UPDATE reservation SET statut = :s WHERE id_reservation = :r"));
    qMaj.bindValue(QStringLiteral(":s"), nouveauStatut);
    qMaj.bindValue(QStringLiteral(":r"), idRes);
    if (!qMaj.exec()) {
        m_db.rollback();
        erreur = QStringLiteral("Erreur lors du changement de statut : ") + qMaj.lastError().text();
        return false;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        erreur = QStringLiteral("Impossible de valider la mise à jour.");
        return false;
    }

    if (idClientTitulaire > 0) {
        QString libelle = (nouveauStatut == QStringLiteral("confirmee"))
                              ? QStringLiteral("CONFIRMÉE — Votre billet PDF est disponible")
                              : ((nouveauStatut == QStringLiteral("annulee"))
                                     ? QStringLiteral("ANNULÉE")
                                     : QStringLiteral("EN ATTENTE"));
        QString ignoreErr;
        envoyerMessageBoite(
            idClientTitulaire,
            QStringLiteral("client"),
            0,
            QStringLiteral("Administration Vols"),
            QStringLiteral("noreply@billets-avion.mg"),
            QStringLiteral("Réservation"),
            QStringLiteral("Mise à jour réservation %1 : %2").arg(reference, libelle),
            QStringLiteral("Le statut de votre réservation %1 est désormais : %2.\n"
                           "Consultez l'onglet « Mes réservations » pour les détails et le billet PDF.")
                .arg(reference, libelle),
            ignoreErr);
    }

    erreur.clear();
    return true;
}

bool DatabaseManager::annulerReservationClient(const QString &reference,
                                               int idUtilisateur,
                                               QString &erreur)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id_utilisateur, statut FROM reservation WHERE reference = :ref"));
    q.bindValue(QStringLiteral(":ref"), reference);
    if (!q.exec() || !q.next()) {
        erreur = QStringLiteral("Réservation introuvable.");
        return false;
    }
    if (q.value(0).toInt() != idUtilisateur) {
        erreur = QStringLiteral("Cette réservation ne vous appartient pas.");
        return false;
    }
    if (q.value(1).toString() == QStringLiteral("annulee")) {
        erreur = QStringLiteral("Cette réservation est déjà annulée.");
        return false;
    }
    q.finish();

    if (!changerStatutReservation(reference, QStringLiteral("annulee"), erreur))
        return false;

    // Envoie une notification non lue (point rouge) à l'Admin quand un client annule
    QString ignoreErr;
    envoyerMessageBoite(
        0,
        QStringLiteral("admin"),
        idUtilisateur,
        QStringLiteral("Client #%1").arg(idUtilisateur),
        QString(),
        QStringLiteral("Annulation"),
        QStringLiteral("Réservation annulée par le client : %1").arg(reference),
        QStringLiteral("Le client #%1 vient d'annuler sa réservation %2.").arg(idUtilisateur).arg(reference),
        ignoreErr);

    return true;
}

bool DatabaseManager::obtenirDetailsReservation(const QString &reference,
                                                DetailsReservation &details,
                                                QString &erreur)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT r.id_reservation, r.reference, r.id_utilisateur, u.nom, u.prenom, u.email, "
        "r.nb_places, r.prix_total, r.statut, r.nb_escales, r.date_reservation, "
        "r.mode_paiement, r.contact_paiement, r.reference_paiement "
        "FROM reservation r "
        "JOIN utilisateur u ON u.id_utilisateur = r.id_utilisateur "
        "WHERE r.reference = :ref"));
    q.bindValue(QStringLiteral(":ref"), reference);

    if (!q.exec() || !q.next()) {
        erreur = QStringLiteral("Impossible de charger la réservation ") + reference;
        return false;
    }

    details = DetailsReservation();
    details.idReservation = q.value(0).toInt();
    details.reference = q.value(1).toString();
    details.idUtilisateur = q.value(2).toInt();
    details.nomClient = q.value(3).toString();
    details.prenomClient = q.value(4).toString();
    details.emailClient = q.value(5).toString();
    details.nbPlaces = q.value(6).toInt();
    details.prixTotal = q.value(7).toDouble();
    details.statut = q.value(8).toString();
    details.nbEscales = q.value(9).toInt();
    details.dateReservation = q.value(10).toDateTime();
    details.modePaiement = q.value(11).toString();
    details.contactPaiement = q.value(12).toString();
    details.referencePaiement = q.value(13).toString();
    q.finish();

    QSqlQuery qVols(m_db);
    qVols.prepare(QStringLiteral(
        "SELECT v.id_vol, v.numero_vol, v.compagnie, v.date_depart, v.date_arrivee, "
        "v.distance_km, v.duree_minutes, v.prix, v.places_disponibles, "
        "ad.id_aeroport, ad.code_iata, ad.nom_aeroport, ad.ville, ad.pays, ad.code_pays, "
        "aa.id_aeroport, aa.code_iata, aa.nom_aeroport, aa.ville, aa.pays, aa.code_pays "
        "FROM reservation_vol rv "
        "JOIN vol v ON v.id_vol = rv.id_vol "
        "JOIN aeroport ad ON ad.id_aeroport = v.id_aeroport_depart "
        "JOIN aeroport aa ON aa.id_aeroport = v.id_aeroport_arrivee "
        "WHERE rv.id_reservation = :r "
        "ORDER BY rv.ordre_troncon ASC"));
    qVols.bindValue(QStringLiteral(":r"), details.idReservation);
    if (qVols.exec()) {
        while (qVols.next()) {
            TronconVol t;
            t.idVolExistant = qVols.value(0).toInt();
            t.numeroVol = qVols.value(1).toString();
            t.compagnie = qVols.value(2).toString();
            t.dateDepart = qVols.value(3).toDateTime();
            t.dateArrivee = qVols.value(4).toDateTime();
            t.distanceKm = qVols.value(5).toInt();
            t.dureeMinutes = qVols.value(6).toInt();
            t.prixUnitaire = qVols.value(7).toDouble();
            t.placesDisponibles = qVols.value(8).toInt();

            t.depart.idAeroport = qVols.value(9).toInt();
            t.depart.codeIata = qVols.value(10).toString();
            t.depart.nomAeroport = qVols.value(11).toString();
            t.depart.ville = qVols.value(12).toString();
            t.depart.pays = qVols.value(13).toString();
            t.depart.codePays = qVols.value(14).toString();

            t.arrivee.idAeroport = qVols.value(15).toInt();
            t.arrivee.codeIata = qVols.value(16).toString();
            t.arrivee.nomAeroport = qVols.value(17).toString();
            t.arrivee.ville = qVols.value(18).toString();
            t.arrivee.pays = qVols.value(19).toString();
            t.arrivee.codePays = qVols.value(20).toString();

            details.troncons.append(t);
        }
    }

    QSqlQuery qPass(m_db);
    qPass.prepare(QStringLiteral(
        "SELECT nom, prenom, date_naissance, numero_passeport, siege "
        "FROM passager WHERE id_reservation = :r ORDER BY id_passager ASC"));
    qPass.bindValue(QStringLiteral(":r"), details.idReservation);
    if (qPass.exec()) {
        while (qPass.next()) {
            PassagerInfo p;
            p.nom = qPass.value(0).toString();
            p.prenom = qPass.value(1).toString();
            p.dateNaissance = qPass.value(2).toDate();
            p.numeroPasseport = qPass.value(3).toString();
            p.siege = qPass.value(4).toString();
            details.passagers.append(p);
        }
    }

    erreur.clear();
    return true;
}

StatistiquesAdmin DatabaseManager::obtenirStatistiquesAdmin()
{
    StatistiquesAdmin s;
    QSqlQuery q(m_db);

    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM vol WHERE statut_vol = 'programme'"))
        && q.next()) {
        s.nbVolsProgrammes = q.value(0).toInt();
    }

    if (q.exec(QStringLiteral(
            "SELECT COUNT(*), "
            "IFNULL(SUM(statut = 'en_attente'), 0), "
            "IFNULL(SUM(statut = 'confirmee'), 0), "
            "IFNULL(SUM(statut = 'annulee'), 0), "
            "IFNULL(SUM(CASE WHEN statut = 'confirmee' THEN prix_total ELSE 0 END), 0), "
            "IFNULL(MAX(CASE WHEN statut = 'en_attente' THEN id_reservation ELSE 0 END), 0) "
            "FROM reservation"))
        && q.next()) {
        s.totalReservations = q.value(0).toInt();
        s.nbEnAttente = q.value(1).toInt();
        s.nbConfirmees = q.value(2).toInt();
        s.nbAnnulees = q.value(3).toInt();
        s.chiffreAffairesConfirme = q.value(4).toDouble();
        s.dernierIdEnAttente = q.value(5).toInt();
    }

    return s;
}

QString DatabaseManager::empreinteReservationsClient(int idUtilisateur)
{
    if (idUtilisateur <= 0)
        return QString();

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT reference, statut FROM reservation "
        "WHERE id_utilisateur = :u ORDER BY id_reservation DESC"));
    q.bindValue(QStringLiteral(":u"), idUtilisateur);
    if (!q.exec())
        return QString();

    QStringList morceaux;
    while (q.next()) {
        morceaux << (q.value(0).toString() + QLatin1Char(':') + q.value(1).toString());
    }
    return morceaux.join(QLatin1Char(';'));
}

static QString echapperChampCsv(const QString &champ)
{
    QString propre = champ;
    propre.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + propre + QLatin1Char('"');
}

bool DatabaseManager::exporterReservationsCsv(const QString &cheminCsv, QString &erreur)
{
    QFile fichier(cheminCsv);
    if (!fichier.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        erreur = QStringLiteral("Impossible de créer le fichier CSV.");
        return false;
    }

    // J'écris le BOM UTF-8 pour qu'Excel et LibreOffice affichent bien les accents
    fichier.write("\xEF\xBB\xBF");
    QTextStream flux(&fichier);
    flux.setEncoding(QStringConverter::Utf8);

    flux << "Reference;Date reservation;Client;Email;Vols;Trajet;Places;Passagers et Passeports;Prix total (EUR);Statut\n";

    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "SELECT r.reference, r.date_reservation, CONCAT(u.prenom, ' ', u.nom), u.email, "
        "(SELECT GROUP_CONCAT(v.numero_vol ORDER BY rv.ordre_troncon SEPARATOR ' + ') "
        " FROM reservation_vol rv JOIN vol v ON v.id_vol = rv.id_vol "
        " WHERE rv.id_reservation = r.id_reservation), "
        "(SELECT GROUP_CONCAT(CONCAT(ad.ville, ' (', ad.code_iata, ') -> ', aa.ville, ' (', aa.code_iata, ')') "
        " ORDER BY rv.ordre_troncon SEPARATOR ' | ') "
        " FROM reservation_vol rv JOIN vol v ON v.id_vol = rv.id_vol "
        " JOIN aeroport ad ON ad.id_aeroport = v.id_aeroport_depart "
        " JOIN aeroport aa ON aa.id_aeroport = v.id_aeroport_arrivee "
        " WHERE rv.id_reservation = r.id_reservation), "
        "r.nb_places, "
        "(SELECT GROUP_CONCAT(CONCAT(p.prenom, ' ', p.nom, ' [', p.numero_passeport, ' - ', p.siege, ']') "
        " ORDER BY p.id_passager SEPARATOR ', ') "
        " FROM passager p WHERE p.id_reservation = r.id_reservation), "
        "r.prix_total, r.statut "
        "FROM reservation r "
        "JOIN utilisateur u ON u.id_utilisateur = r.id_utilisateur "
        "ORDER BY r.id_reservation DESC");

    if (!q.exec(sql)) {
        erreur = QStringLiteral("Erreur SQL pendant l'export CSV : ") + q.lastError().text();
        return false;
    }

    while (q.next()) {
        QStringList colonnes;
        colonnes << echapperChampCsv(q.value(0).toString())
                 << echapperChampCsv(q.value(1).toDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")))
                 << echapperChampCsv(q.value(2).toString())
                 << echapperChampCsv(q.value(3).toString())
                 << echapperChampCsv(q.value(4).toString())
                 << echapperChampCsv(q.value(5).toString())
                 << QString::number(q.value(6).toInt())
                 << echapperChampCsv(q.value(7).toString())
                 << QString::number(q.value(8).toDouble(), 'f', 2)
                 << echapperChampCsv(q.value(9).toString());
        flux << colonnes.join(QLatin1Char(';')) << "\n";
    }

    fichier.close();
    erreur.clear();
    return true;
}

void DatabaseManager::remplirModeleReservationsClient(QSqlQueryModel *modele,
                                                      int idUtilisateur,
                                                      const QString &codeMonnaie)
{
    if (!modele)
        return;

    const double taux = Constantes::tauxMonnaieDepuisEur(codeMonnaie);
    const int dec = (codeMonnaie == QStringLiteral("MGA")) ? 0 : 2;
    const QString sym = Constantes::symboleMonnaie(codeMonnaie);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT r.reference AS 'Référence', "
        "DATE_FORMAT(r.date_reservation, '%d/%m/%Y %H:%i') AS 'Date', "
        "(SELECT GROUP_CONCAT(CONCAT(ad.ville, ' (', ad.code_iata, ') → ', aa.ville, ' (', aa.code_iata, ')') "
        " ORDER BY rv.ordre_troncon SEPARATOR ' | ') "
        " FROM reservation_vol rv JOIN vol v ON v.id_vol = rv.id_vol "
        " JOIN aeroport ad ON ad.id_aeroport = v.id_aeroport_depart "
        " JOIN aeroport aa ON aa.id_aeroport = v.id_aeroport_arrivee "
        " WHERE rv.id_reservation = r.id_reservation) AS 'Itinéraire', "
        "(SELECT GROUP_CONCAT(v.numero_vol ORDER BY rv.ordre_troncon SEPARATOR ' + ') "
        " FROM reservation_vol rv JOIN vol v ON v.id_vol = rv.id_vol "
        " WHERE rv.id_reservation = r.id_reservation) AS 'Vol(s)', "
        "r.nb_places AS 'Places', "
        "CONCAT(FORMAT(r.prix_total * :taux, :dec), ' ', :sym) AS 'Total', "
        "CASE WHEN r.reference_paiement = '' THEN '—' "
        "     ELSE CONCAT(r.mode_paiement, ' (', r.reference_paiement, ')') END AS 'Paiement', "
        "CASE r.statut "
        "  WHEN 'confirmee' THEN 'Confirmée' "
        "  WHEN 'annulee' THEN 'Annulée' "
        "  ELSE 'En attente' END AS 'Statut' "
        "FROM reservation r "
        "WHERE r.id_utilisateur = :u "
        "ORDER BY r.id_reservation DESC"));
    q.bindValue(QStringLiteral(":taux"), taux);
    q.bindValue(QStringLiteral(":dec"), dec);
    q.bindValue(QStringLiteral(":sym"), sym);
    q.bindValue(QStringLiteral(":u"), idUtilisateur);
    q.exec();
    modele->setQuery(std::move(q));
}

void DatabaseManager::remplirModeleReservationsAdmin(QSqlQueryModel *modele,
                                                     const QString &filtreStatut,
                                                     const QString &codeMonnaie)
{
    if (!modele)
        return;

    const double taux = Constantes::tauxMonnaieDepuisEur(codeMonnaie);
    const int dec = (codeMonnaie == QStringLiteral("MGA")) ? 0 : 2;
    const QString sym = Constantes::symboleMonnaie(codeMonnaie);

    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "SELECT r.reference AS 'Référence', "
        "DATE_FORMAT(r.date_reservation, '%d/%m/%Y %H:%i') AS 'Date', "
        "CONCAT(u.prenom, ' ', u.nom) AS 'Client', "
        "(SELECT GROUP_CONCAT(v.numero_vol ORDER BY rv.ordre_troncon SEPARATOR ' + ') "
        " FROM reservation_vol rv JOIN vol v ON v.id_vol = rv.id_vol "
        " WHERE rv.id_reservation = r.id_reservation) AS 'Vol(s)', "
        "r.nb_places AS 'Places', "
        "CONCAT(FORMAT(r.prix_total * :taux, :dec), ' ', :sym) AS 'Total', "
        "CASE WHEN r.reference_paiement = '' THEN '—' "
        "     ELSE CONCAT(r.mode_paiement, ' : ', r.reference_paiement) END AS 'Réf. Paiement', "
        "CASE r.statut "
        "  WHEN 'confirmee' THEN 'Confirmée' "
        "  WHEN 'annulee' THEN 'Annulée' "
        "  ELSE 'En attente' END AS 'Statut' "
        "FROM reservation r "
        "JOIN utilisateur u ON u.id_utilisateur = r.id_utilisateur ");

    if (!filtreStatut.isEmpty())
        sql += QStringLiteral("WHERE r.statut = :st ");

    sql += QStringLiteral("ORDER BY r.id_reservation DESC");
    q.prepare(sql);
    q.bindValue(QStringLiteral(":taux"), taux);
    q.bindValue(QStringLiteral(":dec"), dec);
    q.bindValue(QStringLiteral(":sym"), sym);
    if (!filtreStatut.isEmpty())
        q.bindValue(QStringLiteral(":st"), filtreStatut);
    q.exec();
    modele->setQuery(std::move(q));
}

void DatabaseManager::remplirModeleVolsAdmin(QSqlQueryModel *modele, const QString &codeMonnaie)
{
    if (!modele)
        return;

    const double taux = Constantes::tauxMonnaieDepuisEur(codeMonnaie);
    const int dec = (codeMonnaie == QStringLiteral("MGA")) ? 0 : 2;
    const QString sym = Constantes::symboleMonnaie(codeMonnaie);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT v.id_vol, "
        "v.numero_vol AS 'N° Vol', "
        "v.compagnie AS 'Compagnie', "
        "CONCAT(ad.ville, ' (', ad.code_iata, ')') AS 'Départ', "
        "CONCAT(aa.ville, ' (', aa.code_iata, ')') AS 'Arrivée', "
        "DATE_FORMAT(v.date_depart, '%d/%m/%Y %H:%i') AS 'Date départ', "
        "DATE_FORMAT(v.date_arrivee, '%d/%m/%Y %H:%i') AS 'Date arrivée', "
        "CONCAT(v.distance_km, ' km') AS 'Distance', "
        "CONCAT(FORMAT(v.prix * :taux, :dec), ' ', :sym) AS 'Prix', "
        "CONCAT(v.places_disponibles, ' / ', v.capacite) AS 'Places disp.' "
        "FROM vol v "
        "JOIN aeroport ad ON ad.id_aeroport = v.id_aeroport_depart "
        "JOIN aeroport aa ON aa.id_aeroport = v.id_aeroport_arrivee "
        "WHERE v.statut_vol = 'programme' "
        "ORDER BY v.date_depart ASC"));
    q.bindValue(QStringLiteral(":taux"), taux);
    q.bindValue(QStringLiteral(":dec"), dec);
    q.bindValue(QStringLiteral(":sym"), sym);
    q.exec();
    modele->setQuery(std::move(q));
}

void DatabaseManager::remplirModeleAdministrateurs(QSqlQueryModel *modele)
{
    if (!modele)
        return;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id_utilisateur AS 'ID', "
        "nom AS 'Nom', "
        "prenom AS 'Prénom', "
        "email AS 'Email', "
        "role AS 'Rôle', "
        "DATE_FORMAT(date_creation, '%d/%m/%Y %H:%i') AS 'Créé le' "
        "FROM utilisateur "
        "WHERE role IN ('admin', 'super_admin') "
        "ORDER BY role DESC, id_utilisateur ASC"));
    q.exec();
    modele->setQuery(std::move(q));
}

void DatabaseManager::remplirModeleClientsAdmin(QSqlQueryModel *modele,
                                                const QString &filtreRecherche)
{
    if (!modele)
        return;

    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "SELECT id_utilisateur AS 'ID', "
        "nom AS 'Nom', "
        "prenom AS 'Prénom', "
        "email AS 'Email', "
        "CASE WHEN est_bloque = 1 THEN 'Bloqué' ELSE 'Actif' END AS 'Statut', "
        "DATE_FORMAT(date_creation, '%d/%m/%Y %H:%i') AS 'Inscrit le' "
        "FROM utilisateur "
        "WHERE role = 'client' ");

    QString motif = filtreRecherche.trimmed();
    if (!motif.isEmpty()) {
        sql += QStringLiteral(
            "AND (nom LIKE :f OR prenom LIKE :f OR email LIKE :f) ");
    }
    sql += QStringLiteral("ORDER BY id_utilisateur DESC");

    q.prepare(sql);
    if (!motif.isEmpty())
        q.bindValue(QStringLiteral(":f"), QLatin1Char('%') + motif + QLatin1Char('%'));
    q.exec();
    modele->setQuery(std::move(q));
}

bool DatabaseManager::envoyerMessageBoite(int idDestinataire,
                                          const QString &roleCible,
                                          int idExpediteur,
                                          const QString &nomExpediteur,
                                          const QString &emailExpediteur,
                                          const QString &categorie,
                                          const QString &sujet,
                                          const QString &contenu,
                                          QString &erreur)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO notification_message "
        "(id_destinataire, role_cible, id_expediteur, nom_expediteur, email_expediteur, categorie, sujet, contenu) "
        "VALUES (:dest, :role, :exp, :nom, :mail, :cat, :suj, :corps)"));
    q.bindValue(QStringLiteral(":dest"), idDestinataire);
    q.bindValue(QStringLiteral(":role"),
                (roleCible == QStringLiteral("admin") || roleCible == QStringLiteral("super_admin"))
                    ? QStringLiteral("admin")
                    : QStringLiteral("client"));
    q.bindValue(QStringLiteral(":exp"), idExpediteur);
    q.bindValue(QStringLiteral(":nom"),
                (nomExpediteur.trimmed().isEmpty() ? QStringLiteral("Visiteur")
                                                   : nomExpediteur.trimmed()).left(95));
    q.bindValue(QStringLiteral(":mail"), emailExpediteur.trimmed().left(95));
    q.bindValue(QStringLiteral(":cat"), categorie.trimmed().left(38));
    q.bindValue(QStringLiteral(":suj"), sujet.trimmed().left(155));
    q.bindValue(QStringLiteral(":corps"), contenu.trimmed());

    if (!q.exec()) {
        erreur = QStringLiteral("Impossible d'envoyer le message : ") + q.lastError().text();
        return false;
    }
    erreur.clear();
    return true;
}

static QString clauseDestinataireBoite(const QString &role, bool &lierIdUtilisateur)
{
    if (role == QStringLiteral("admin") || role == QStringLiteral("super_admin")) {
        lierIdUtilisateur = false;
        return QStringLiteral("role_cible = 'admin'");
    }
    lierIdUtilisateur = true;
    return QStringLiteral("role_cible = 'client' AND id_destinataire = :u");
}

QList<MessageBoiteReception> DatabaseManager::chargerMessagesBoite(int idUtilisateur,
                                                                   const QString &role,
                                                                   bool seulementNonLus)
{
    QList<MessageBoiteReception> liste;
    if (idUtilisateur <= 0)
        return liste;

    bool lierU = false;
    QString sql = QStringLiteral(
        "SELECT id_message, id_destinataire, role_cible, id_expediteur, "
        "nom_expediteur, email_expediteur, categorie, sujet, contenu, est_lu, date_envoi "
        "FROM notification_message WHERE ") + clauseDestinataireBoite(role, lierU);

    if (seulementNonLus)
        sql += QStringLiteral(" AND est_lu = 0");

    sql += QStringLiteral(" ORDER BY id_message DESC LIMIT 150");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (lierU)
        q.bindValue(QStringLiteral(":u"), idUtilisateur);
    if (!q.exec())
        return liste;

    while (q.next()) {
        MessageBoiteReception m;
        m.idMessage = q.value(0).toInt();
        m.idDestinataire = q.value(1).toInt();
        m.roleCible = q.value(2).toString();
        m.idExpediteur = q.value(3).toInt();
        m.nomExpediteur = q.value(4).toString();
        m.emailExpediteur = q.value(5).toString();
        m.categorie = q.value(6).toString();
        m.sujet = q.value(7).toString();
        m.contenu = q.value(8).toString();
        m.estLu = (q.value(9).toInt() != 0);
        m.dateEnvoi = q.value(10).toDateTime();
        liste.append(m);
    }
    return liste;
}

int DatabaseManager::compterMessagesNonLus(int idUtilisateur, const QString &role)
{
    if (idUtilisateur <= 0)
        return 0;

    bool lierU = false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM notification_message WHERE ")
              + clauseDestinataireBoite(role, lierU) + QStringLiteral(" AND est_lu = 0"));
    if (lierU)
        q.bindValue(QStringLiteral(":u"), idUtilisateur);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

void DatabaseManager::marquerMessageCommeLu(int idMessage)
{
    if (idMessage <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE notification_message SET est_lu = 1 WHERE id_message = :id"));
    q.bindValue(QStringLiteral(":id"), idMessage);
    q.exec();
}

void DatabaseManager::toutMarquerCommeLu(int idUtilisateur, const QString &role)
{
    if (idUtilisateur <= 0)
        return;

    bool lierU = false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE notification_message SET est_lu = 1 WHERE ")
              + clauseDestinataireBoite(role, lierU) + QStringLiteral(" AND est_lu = 0"));
    if (lierU)
        q.bindValue(QStringLiteral(":u"), idUtilisateur);
    q.exec();
}

void DatabaseManager::supprimerMessageBoite(int idMessage)
{
    if (idMessage <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM notification_message WHERE id_message = :id"));
    q.bindValue(QStringLiteral(":id"), idMessage);
    q.exec();
}

void DatabaseManager::viderBoiteReception(int idUtilisateur, const QString &role)
{
    if (idUtilisateur <= 0)
        return;

    bool lierU = false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM notification_message WHERE ")
              + clauseDestinataireBoite(role, lierU));
    if (lierU)
        q.bindValue(QStringLiteral(":u"), idUtilisateur);
    q.exec();
}

