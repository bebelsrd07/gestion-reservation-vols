# Guide Ultime d'Installation, d'Utilisation et de Soutenance
## Application de Réservation de Billets d'Avion (C++17 / Qt 6 / MySQL) + Générateur de Passeports

Ce guide contient **tout ce dont vous avez besoin** de A à Z : tous les identifiants, comment importer la base MySQL, quoi compiler et lancer, comment configurer l'envoi d'email réel, et le déroulé exact de la démonstration à **deux fenêtres simultanées (Client + Admin)** sur un seul PC.

---

## Sommaire
1. [Tous les identifiants à connaître (Application & MySQL)](#1-tous-les-identifiants-à-connaître-application--mysql)
2. [Ce que contient le dépôt (les 2 projets)](#2-ce-que-contient-le-dépôt-les-2-projets)
3. [Étape 1 — Démarrer MySQL et importer la base de données](#3-étape-1--démarrer-mysql-et-importer-la-base-de-données)
4. [Étape 2 — Configurer `config.ini` (MySQL & Email Gmail SMTP)](#4-étape-2--configurer-configini-mysql--email-gmail-smtp)
5. [Étape 3 — Compiler et lancer l'application dans Qt Creator](#5-étape-3--compiler-et-lancer-lapplication-dans-qt-creator)
6. [Étape 4 — Utiliser le générateur de faux passeports (sans OCR)](#6-étape-4--utiliser-le-générateur-de-faux-passeports-sans-ocr)
7. [Étape 5 — Scénario complet de démonstration devant le jury (2 instances)](#7-étape-5--scénario-complet-de-démonstration-devant-le-jury-2-instances)
8. [Aide-mémoire pour les questions du jury (Code & Formules)](#8-aide-mémoire-pour-les-questions-du-jury-code--formules)
9. [Dépannage rapide (FAQ)](#9-dépannage-rapide-faq)

---

## 1. Tous les identifiants à connaître (Application & MySQL)

### A. Comptes utilisateurs pré-enregistrés dans l'application
Ces trois comptes sont créés automatiquement dès que vous importez `reservation_vol_db.sql` (leurs mots de passe sont stockés **hachés en SHA-256 avec sel aléatoire** dans la table `utilisateur`) :

| Rôle | Email (Identifiant) | Mot de passe | Ce que ce compte peut faire |
|---|---|---|---|
| **Super Administrateur** | `superadmin@billets-avion.mg` | `SuperAdmin2026!` | **Tout gérer** : tableau de bord, confirmer/annuler les réservations, gérer les vols, configurer le SMTP, exporter en CSV, **et créer / supprimer des comptes Administrateurs**. |
| **Administrateur** | `admin@billets-avion.mg` | `Admin2026!` | Gérer le tableau de bord, confirmer/annuler les réservations, gérer les vols, configurer le SMTP et exporter en CSV (**sans** pouvoir créer ni supprimer d'autres admins). |
| **Client de démo** | `client@billets-avion.mg` | `Client2026!` | Rechercher des vols, réserver pour 1 à 9 passagers avec import de passeports, suivre ses réservations, annuler une demande et télécharger son billet PDF. |
| **Visiteur (sans compte)** | *(Bouton « Continuer en mode visiteur »)* | *(Aucun)* | Consulter et rechercher tous les vols et itinéraires avec escales (la connexion n'est demandée qu'au moment de réserver). |

> Vous pouvez également créer autant de nouveaux comptes clients que vous voulez depuis l'onglet **Inscription client** de l'écran d'accueil.

### B. Paramètres MySQL par défaut
Sans fichier `config.ini`, l'application tente automatiquement de se connecter en local avec :
- **Hôte** : `localhost` (`127.0.0.1`)
- **Port** : `3306`
- **Nom de la base** : `reservation_vol_db`
- **Utilisateur** : `root`
- **Mot de passe** : essaie automatiquement `passwd`, puis un mot de passe vide `""` (cas par défaut de XAMPP / WAMP / Laragon).
- Si votre MySQL utilise un autre mot de passe, indiquez-le simplement dans `config.ini` (voir [Étape 2](#4-étape-2--configurer-configini-mysql--email-gmail-smtp)).

---

## 2. Ce que contient le dépôt (les 2 projets)

Vous avez **deux outils indépendants** dans ce dépôt :

1. **L'application principale C++ / Qt 6 (`CMakeLists.txt`)** :
   - Fenêtre unique (`QStackedWidget`) conservant le design natif Qt Designer (`mainwindow.ui`), sans fenêtres pop-up dispersées.
2. **Le générateur indépendant de faux passeports (`generateur_passeport/index.html`)** :
   - Application autonome HTML/CSS/JS fonctionnant **100 % hors-ligne** (aucune connexion internet ni serveur requis) : un simple double-clic l'ouvre dans votre navigateur pour fabriquer des passeports `.pdf` ou `.png` prêts à être importés dans l'application Qt.

---

## 3. Étape 1 — Démarrer MySQL et importer la base de données

Avant de lancer l'application Qt, démarrez votre serveur **MySQL** local (XAMPP, WAMP, Laragon, MAMP ou service MySQL système), puis importez le fichier **`reservation_vol_db.sql`** avec la méthode de votre choix :

### Méthode A — Via phpMyAdmin (XAMPP / WAMP / Laragon)
1. Démarrez **Apache** et **MySQL** dans votre panneau XAMPP / WAMP / Laragon.
2. Ouvrez votre navigateur sur : `http://localhost/phpmyadmin`
3. Cliquez directement sur l'onglet **Importer** en haut (pas besoin de créer la base à la main : le script exécute `CREATE DATABASE IF NOT EXISTS reservation_vol_db` tout seul).
4. Cliquez sur **Choisir un fichier** et sélectionnez **`reservation_vol_db.sql`** à la racine du dépôt.
5. Descendez en bas de la page et cliquez sur **Importer / Exécuter**.

### Méthode B — En ligne de commande (Terminal Linux / macOS / CMD Windows)
Ouvrez un terminal dans le dossier du projet (`arena_billets_avion`) et tapez :
```bash
mysql -u root -p < reservation_vol_db.sql
```
*(Appuyez sur Entrée ; tapez votre mot de passe root MySQL s'il vous est demandé, ou appuyez directement sur Entrée sous XAMPP/WAMP s'il est vide).*

### Méthode C — Via MySQL Workbench
1. Ouvrez votre connexion locale `localhost:3306`.
2. Menu **Server** → **Data Import**.
3. Cochez **Import from Self-Contained File** et sélectionnez **`reservation_vol_db.sql`**.
4. Cliquez sur **Start Import**.

### Ce qui est créé automatiquement par ce script :
- **7 tables propres en minuscules** : `utilisateur`, `aeroport`, `liaison`, `vol`, `reservation`, `reservation_vol`, `passager` (les anciennes tables inutilisées sont nettoyées automatiquement).
- **48 aéroports** (Antananarivo `TNR`, Fianarantsoa `WFI`, Nosy Be `NOS`, Toamasina `TMM`, Mahajanga `MJN`, Toliara `TLE`, Antsiranana `DIE`, Sainte-Marie `SMS`, Taolagnaro `FTU`, plus les grands hubs mondiaux et les villes à **plusieurs aéroports** comme Paris `CDG`/`ORY`, Londres `LHR`/`LGW`, New York `JFK`/`EWR`, Tokyo `HND`/`NRT`, Saint-Denis `RUN`/`ZSE`, Milan `MXP`/`LIN`, Istanbul `IST`/`SAW`).
- **196 liaisons aériennes de référence** et **20 vols programmés dans le futur**.
- Les **3 comptes utilisateurs** (`super_admin`, `admin`, `client`).

---

## 4. Étape 2 — Configurer `config.ini` (MySQL & Email Gmail SMTP)

### A. Créer votre fichier `config.ini`
À la racine du projet, copiez le fichier modèle `config.ini.exemple` et renommez la copie en **`config.ini`** :

```ini
[mysql]
hote=127.0.0.1
port=3306
base=reservation_vol_db
utilisateur=root
mot_de_passe=

[smtp]
serveur=smtp.gmail.com
port=465
email_expediteur=votre.adresse@gmail.com
mot_de_passe_application=xxxx xxxx xxxx xxxx
nom_expediteur=Agence Billets d'Avion
```
- Adaptez `mot_de_passe=` dans `[mysql]` uniquement si votre serveur MySQL local a un mot de passe particulier.
- `config.ini` est déjà ignoré par `.gitignore` pour ne jamais publier vos mots de passe sur GitHub.

### B. Activer l'envoi réel d'emails avec Gmail gratuit (2 minutes)
Lorsqu'un administrateur confirme une réservation, l'application génère le billet PDF et l'envoie **réellement** par email au client. Pour utiliser un compte Gmail gratuit :
1. Connectez-vous à votre compte Google et ouvrez **Gérer votre compte Google** → **Sécurité**.
2. Activez la **Validation en deux étapes** (obligatoire chez Google pour créer un mot de passe SMTP).
3. Dans la barre de recherche en haut de votre compte Google, tapez **« Mots de passe des applications »** (*App passwords*).
4. Donnez un nom (ex. `Projet Qt Avion`) et cliquez sur **Créer** : Google affiche un code de **16 lettres** (ex. `abcd efgh ijkl mnop`).
5. Vous pouvez renseigner votre adresse Gmail et ce code de 16 lettres :
   - **Soit dans `config.ini`** (section `[smtp]`),
   - **Soit directement dans l'application** une fois connecté en Admin : onglet **Administration** → **Email SMTP** → bouton **Enregistrer la configuration SMTP**.

> **Compatible aussi avec Mailtrap Sandbox !**  
> Dans l'onglet **Email SMTP**, mettez `sandbox.smtp.mailtrap.io`, Port `587` ou `2525`, collez votre *Username* Mailtrap dans **Identifiant SMTP (optionnel)**, `contact@billets-avion.mg` dans **Email expéditeur**, et votre *Password* Mailtrap dans **Mot de passe SMTP**.

> **Bon à savoir pour la soutenance** : si le PC de soutenance n'a pas d'accès Internet au moment du clic, l'application **ne plante jamais** : la réservation passe bien au statut **Confirmée**, le **billet PDF unique est généré sur le disque** dans le dossier `billets_generes/`, et un message clair indique simplement que l'envoi réseau SMTP n'a pas pu aboutir.

---

## 5. Étape 3 — Compiler et lancer l'application dans Qt Creator

### A. Compilation dans Qt Creator
1. Lancez **Qt Creator**.
2. Cliquez sur **Fichier** → **Ouvrir un fichier ou un projet...** et sélectionnez **`CMakeLists.txt`** à la racine du dépôt.
3. Cochez votre kit **Desktop Qt 6.x** (MinGW ou MSVC sous Windows, GCC sous Linux, Clang sous macOS) puis cliquez sur **Configurer le projet** (*Configure Project*).
4. Compilez le projet avec le marteau en bas à gauche (**`Ctrl + B`**).
5. Lancez l'application avec le bouton vert Lecture (**`Ctrl + R`**).

### B. Comment ouvrir les 2 instances simultanées sur le même PC (Client + Admin)
> **Pourquoi Windows empêche un second « Build » quand une fenêtre tourne déjà ?**  
> Sous Windows, tant qu'une fenêtre `ReservationBilletsAvion.exe` est ouverte, Windows verrouille le fichier `.exe` en écriture : si vous cliquez à nouveau sur *Compiler (Build)*, le compilateur ne peut pas écraser le `.exe` ouvert. **Il ne faut compiler qu'une seule fois !**

Pour ouvrir votre seconde fenêtre instantanément sans recompiler :
- **Méthode recommandée (en 1 clic dans l'application !)** : lancez l'application une fois (`Ctrl + R`), puis cliquez simplement sur le bouton **« Nouvelle fenêtre »** dans la barre du haut → une 2e instance indépendante s'ouvre immédiatement !
- **Alternative** : double-cliquez une deuxième fois sur `ReservationBilletsAvion.exe` directement dans votre dossier `build/...`.

Placez la **Fenêtre 1 à gauche (Client)** et la **Fenêtre 2 à droite (Admin / Super Admin)**. Les deux fenêtres communiquent avec la même base MySQL locale et se rafraîchissent automatiquement toutes les **4 secondes** (bannière + pastille rouge sur la **Boîte de réception**) !

---

## 6. Étape 4 — Utiliser le générateur de faux passeports (sans OCR)

Lors d'une réservation, chaque passager doit fournir un passeport valide dont le **numéro**, le **nom** et le **prénom** correspondent exactement aux informations saisies.

1. Ouvrez le dossier **`generateur_passeport/`** et double-cliquez sur **`index.html`** (s'ouvre dans Chrome, Edge, Firefox ou Safari, sans Internet).
2. Remplissez :
   - **Nom de famille** (ex. `RASOA`),
   - **Prénom(s)** (ex. `Miora`),
   - **Date de naissance** (ex. `15/08/2003`),
   - **Nationalité / Code pays** (ex. `Madagascar (MDG)`),
   - **Photo d'identité** *(optionnel : vous pouvez importer n'importe quelle image JPG/PNG, sinon une silhouette officielle propre est dessinée automatiquement)*.
3. Un **numéro de passeport structuré aléatoire** est généré automatiquement (ex. `MG4829103`). Vous pouvez cliquer sur **Générer un nouveau N°** ou le copier en un clic.
4. Cliquez au choix sur :
   - **Exporter en PDF (`.pdf`)**, ou
   - **Exporter en image PNG (`.png`)**.
5. Répétez l'opération si votre réservation comporte un 2e ou 3e passager (un passeport par passager).

### Comment fonctionne la vérification sans OCR ? *(Explication simple pour le jury)*
Au moment de l'export, le générateur dessine visuellement la page biométrique du passeport **et** inscrit une balise texte structurée :
```text
PASSEPORT_DATA|NUMERO=MG4829103|NOM=RASOA|PRENOM=Miora|NAISSANCE=2003-08-15|
```
directement dans les métadonnées du fichier `.pdf` ou dans un segment texte standard (`tEXt`) du fichier `.png`.  
Côté C++ (`CalculsVol::lireFichierPasseportSansOcr`), l'application lit les octets du fichier, extrait cette balise sans aucune bibliothèque d'OCR externe, normalise les accents/majuscules et vérifie que le **numéro**, le **nom** et le **prénom** correspondent à 100 % à la fiche passager.

---

## 7. Étape 5 — Scénario complet de démonstration devant le jury (2 instances)

Voici le déroulé idéal pour montrer toutes les fonctionnalités demandées en 5 à 7 minutes, sans aucun temps mort :

### Préparation avant de commencer (1 minute)
1. Vérifiez que **MySQL** est démarré.
2. Ouvrez **`generateur_passeport/index.html`** dans votre navigateur et préparez 2 faux passeports (ex. Passager 1 : `RASOA` / `Miora`, Passager 2 : `RAKOTO` / `Lucas`) exportés l'un en `.pdf` et l'autre en `.png` sur le Bureau.
3. Lancez **2 instances** de `ReservationBilletsAvion` côte à côte :
   - **Fenêtre de droite (Admin)** : connectez-vous avec `superadmin@billets-avion.mg` / `SuperAdmin2026!`.
   - **Fenêtre de gauche (Client)** : restez sur la page d'accès pour commencer la démonstration.

---

### Étape A — Sécurité d'inscription & Mode Visiteur (Fenêtre de gauche)
1. Allez dans l'onglet **Inscription client** et montrez les protections demandées :
   - Essayez de taper un `+` dans le champ email (ex. `jean+alias@gmail.com`) : bloqué dès la saisie (`QValidator`) et rejeté par le validateur métier.
   - Essayez un email jetable (`test@yopmail.com`) : refusé avec un message explicite.
2. Connectez-vous dans l'onglet **Connexion** avec **`client@billets-avion.mg`** / **`Client2026!`** (ou avec le compte client que vous venez de créer).

### Étape B — Affichage automatique des vols, filtres dynamiques, multi-aéroports et escales
1. Dès l'ouverture (même en mode visiteur), **tous les vols programmés s'affichent automatiquement** sans recherche obligatoire. Si vous filtrez (par date, pays ou prix) puis décochez le filtre ou cliquez sur **Réinitialiser les filtres**, toute la liste complète réapparaît immédiatement.
2. Dans **Ville de départ**, tapez simplement `Fianar` → l'autocomplétion propose `Fianarantsoa, Madagascar  MG`. Montrez aussi la **flèche cliquable tout à droite** de la barre qui déroule toutes les villes.
3. Dans **Ville d'arrivée**, sélectionnez `Paris, France  FR` :
   - Montrez que la liste déroulante à droite s'active automatiquement car Paris possède **plusieurs aéroports** (`Tous les aéroports de Paris (2)`, `CDG - Aéroport Paris-Charles de Gaulle`, `ORY - Aéroport de Paris-Orly`).
   - Montrez le bandeau juste en dessous qui **calcule instantanément** la distance en km, la durée théorique et le tarif unitaire dès la sélection !
4. Montrez le calendrier de date : **impossible de sélectionner une date passée** (les jours avant aujourd'hui sont grisés et verrouillés).
5. Réglez **Passagers : 2** (et testez si souhaité le filtre **Prix max / pers.** ou une **Ville d'escale obligatoire** comme `Addis-Abeba, Éthiopie  ET`), puis cliquez sur **Rechercher**.
6. Montrez dans le tableau que les itinéraires proposés comportent **0, 1 ou 2 escales maximum**, avec des dates et heures d'arrivée/départ strictement cohérentes (aucun chevauchement horaire).

### Étape C — Réservation multi-passagers, Plan d'avion interactif ("Seatmaps") & Passeports
1. Sélectionnez un itinéraire et cliquez sur **Réserver l'itinéraire sélectionné** (tout reste dans la même fenêtre !).
2. À droite s'affiche le **Plan d'avion interactif** (`PlanAvionWidget` : cockpit, toilettes `WC`, séparation de cabine **Classe A / Privilège** vs **Cabine Régulière**, issues de secours, sièges libres et occupés `✕`) :
   - Cliquez un 1er siège en **Classe A** (ex. `2A`, ambré) → il s'attribue automatiquement au **Passager 1 (`P1`)** et ajoute dynamiquement le surcoût (`+45 EUR`) au total général !
   - Cliquez un 2e siège en **Cabine Régulière** (ex. `7C`, bleu) → il s'attribue automatiquement au **Passager 2 (`P2`)** (`+0 EUR`).
3. À gauche, remplissez les deux fiches (**Passager 1** et **Passager 2**) et importez leurs passeports générés (`.pdf` ou `.png`).
4. Cliquez sur **Envoyer la réservation (Statut initial : En attente)** : un message récapitulatif est également déposé dans la **Boîte de réception** du client et des administrateurs (pastille rouge `●`).

### Étape D — Notification Admin, contrôle de capacité, PDF unique & Email SMTP (Fenêtre de droite)
1. Regardez la **Fenêtre de droite (Super Admin)** : en moins de 4 secondes, une **bannière verte de notification** apparaît automatiquement en haut (*« Nouvelle réservation client en attente détectée ! »*) et le graphique du **Tableau de bord** ainsi que la liste des **Réservations** sont mis à jour !
2. Dans l'onglet **Réservations**, sélectionnez la réservation en attente et cliquez sur **Confirmer (Vérifier capacité + PDF + Email SMTP)** :
   - Le système vérifie qu'il reste assez de places sur **chaque tronçon** du voyage et décrémente les places disponibles dans une transaction MySQL ;
   - Il génère **un seul fichier PDF** (`billets_generes/Billet_RES-....pdf`) regroupant les **2 passagers**, leurs numéros de passeport, leurs sièges attribués et tous les vols/escales ;
   - Si le SMTP Gmail est renseigné, l'email de confirmation part immédiatement vers l'adresse du client avec le billet PDF en pièce jointe !
3. Montrez également le bouton **Exporter CSV** (qui génère un fichier `.csv` lisible dans Excel / LibreOffice avec tous les détails).

### Étape E — Assistance guidée, Boîte de réception, Gestion Clients & Super Admin
1. Cliquez sur le bouton **« ? Assistance »** dans la barre du haut : choisissez un sujet prédéfini (ex. *Mot de passe oublié / Compte bloqué*), modifiez si besoin l'e-mail prérempli automatiquement et envoyez-le.
2. Sur la **Fenêtre Admin**, la pastille rouge s'allume sur **Boîte de réception** : ouvrez le message du client, répondez-lui directement en 1 clic, puis allez dans l'onglet **Clients & Assistance** pour modifier ses informations, débloquer son compte ou générer un mot de passe provisoire.
3. Enfin, sur la **Fenêtre Super Admin**, ouvrez l'onglet **Super Admin — Gestion des admins** pour montrer le CRUD complet (**Créer**, **Modifier** et **Supprimer** un administrateur), onglet masqué pour un administrateur classique (`admin@billets-avion.mg`).

---

## 8. Aide-mémoire pour les questions du jury (Code & Formules)

### A. Architecture simplifiée des fichiers C++
Tous les fichiers sont courts, séparés par responsabilité et faciles à expliquer :

| Fichier | Explication en une phrase |
|---|---|
| `constantes.h` | Regroupe toutes les constantes nommées (`constexpr`) pour n'avoir **aucun nombre magique** dans le code. |
| `modeles.h` | Contient les structures de données simples (`AeroportInfo`, `TronconVol`, `ItinerairePropose`, `PassagerInfo`, `DetailsReservation`). |
| `calculsvol.h / .cpp` | Contient la formule de Haversine, le calcul de durée/prix, l'algorithme de recherche d'escales (1 à 2 max), la validation stricte des emails et la lecture de passeport sans OCR. |
| `databasemanager.h / .cpp` | Gère toute la communication MySQL (`QSqlQuery` préparées contre les injections SQL, hachage SHA-256 salé, transactions atomiques à la réservation et à la confirmation). |
| `billetpdf.h / .cpp` | Dessine le billet électronique PDF unique multi-passagers au format A4 avec `QPdfWriter` et `QPainter`. |
| `smtpclient.h / .cpp` | Implémente le protocole SMTP sécurisé sur socket TLS (`QSslSocket`, port `465` SSL ou `587` STARTTLS) pour envoyer l'email MIME avec le PDF encodé en Base64. |
| `mainwindow.ui / .h / .cpp` | Gère la fenêtre unique (`QStackedWidget`), les validateurs de saisie (`QRegularExpressionValidator`) et le minuteur (`QTimer`) de synchronisation entre instances. |
| `main.cpp` | Point d'entrée minimal : ouvre la connexion MySQL (avec message clair en cas d'erreur) et affiche `MainWindow`. |

### B. Formules exactes de calcul (`constantes.h` & `calculsvol.cpp`)
1. **Distance entre deux aéroports** :
   - Si le couple d'aéroports figure dans la table MySQL `liaison`, on utilise sa distance de référence ; sinon on applique la **formule orthodromique de Haversine** avec le rayon terrestre moyen `RAYON_TERRE_KM = 6371.0 km`.
2. **Durée de vol d'un tronçon (en minutes)** :
   $$\text{Durée} = \max\left(45,\ \text{arrondi}\left(\frac{\text{distance\_km}}{800\text{ km/h}} \times 60\right) + 30\text{ min}\right)$$
   *(Vitesse de croisière moyenne `800 km/h` + `30 min` forfaitaires de roulage/décollage/approche, avec un minimum incompressible de `45 min`).*
3. **Prix unitaire d'un tronçon par passager (en EUR)** :
   $$\text{Prix} = 35{,}00\text{ EUR} + (\text{distance\_km} \times 0{,}085\text{ EUR/km})$$
4. **Cohérence horaire des escales** :
   - Nombre d'escales borné par `NB_ESCALES_MAX = 2` (soit 3 tronçons maximum).
   - Pour chaque correspondance entre le tronçon $i$ et le tronçon $i+1$, on exige strictement :
     $$60\text{ min} \le (\text{dateDépart}_{i+1} - \text{dateArrivée}_i) \le 720\text{ min}$$
     ce qui garantit mathématiquement l'absence totale de chevauchement de dates ou d'heures.

---

## 9. Dépannage rapide (FAQ)

- **Message « Driver not loaded » (`QMYSQL`) au lancement** :
  - Sous **Linux (Ubuntu/Debian)** : installez le paquet `sudo apt install libqt6sql6-mysql` (ou copiez `libqsqlmysql.so` dans le dossier `sqldrivers` de votre kit Qt 6).
  - Sous **Windows** : assurez-vous que `libmysql.dll` (présent dans `C:\xampp\mysql\lib\` ou `C:\Program Files\MySQL\...\lib\`) est accessible dans le `PATH` ou copié à côté de `ReservationBilletsAvion.exe`.
- **Message « Access denied for user 'root'@'localhost' »** :
  - Ouvrez (ou créez) `config.ini` à la racine du projet et mettez le bon mot de passe sur la ligne `mot_de_passe=...` de la section `[mysql]`.
- **L'email Gmail affiche « Authentification SMTP refusée »** :
  - N'utilisez **pas** votre mot de passe Gmail habituel : Google exige obligatoirement un **Mot de passe d'application de 16 lettres** généré après activation de la validation en 2 étapes (voir [Étape 2.B](#b-activer-lenvoi-réel-demails-avec-gmail-gratuit-2-minutes)).
