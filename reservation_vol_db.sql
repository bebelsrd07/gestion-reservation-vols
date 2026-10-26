DROP DATABASE IF EXISTS reservation_vol_db;
CREATE DATABASE reservation_vol_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE reservation_vol_db;

CREATE TABLE UTILISATEUR(
    id_utilisateur INT AUTO_INCREMENT PRIMARY KEY,
    nom_utilisateur VARCHAR(50) NOT NULL,
    prenom_utilisateur VARCHAR(50) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    mot_de_passe VARCHAR(255) NOT NULL,
    role VARCHAR(20) DEFAULT 'Client'
) ENGINE=InnoDB;

CREATE TABLE VOL(
    id_vol INT AUTO_INCREMENT PRIMARY key,
    numero_vol VARCHAR(20) NOT NULL,
    compagnie VARCHAR(50) NOT NULL,
    ville_depart VARCHAR(50) NOT NULL,
    ville_arrevee VARCHAR(50) NOT NULL,
    date_depart DATETIME NOT NULL,
    date_arrivee DATETIME NOT NULL,
    prix DECIMAL(10,2) NOT NULL CHECK (prix > 0),
    places_disponibles INT NOT NULL CHECK (places_disponibles >= 0)
) ENGINE=InnoDB;

CREATE TABLE PASSAGER(
    id_passager INT AUTO_INCREMENT PRIMARY KEY,
    nom_utilisateur VARCHAR(50) NOT NULL,
    prenom_passager VARCHAR(50) NOT NULL,
    date_naissance DATE NOT NULL,
    numero_document VARCHAR(50) NOT NULL
) ENGINE = InnoDB;

CREATE TABLE RESERVATION(
    id_reservation INT AUTO_INCREMENT PRIMARY KEY,
    reference VARCHAR(20) NOT NULL UNIQUE,
    date_reservation DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    nombre_billets INT NOT NULL CHECK (nombre_billets > 0),
    prix_total DECIMAL(10,2) NOT NULL,
    statut VARCHAR(20) NOT NULL DEFAULT 'Confirmée',
    id_utilisateur INT NOT NULL,
    id_vol INT NOT NULL,
    FOREIGN KEY (id_utilisateur) REFERENCES UTILISATEUR(id_utilisateur) ON DELETE CASCADE,
    FOREIGN KEY (id_vol) REFERENCES VOL(id_vol) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE INCLURE(
    id_reservation INT NOT NULL,
    id_passager INT NOT NULL,
    PRIMARY KEY (id_reservation, id_passager),
    FOREIGN KEY (id_reservation) REFERENCES RESERVATION(id_reservation) ON DELETE CASCADE,
    FOREIGN KEY (id_passager) REFERENCES PASSAGER(id_passager) ON DELETE CASCADE
) ENGINE = InnoDB;