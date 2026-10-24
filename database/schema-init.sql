-- Base de données MySQL pour l'application C++ / Qt
CREATE DATABASE IF NOT EXISTS reservation_vol_db
CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE reservation_vol_db;

-- 1. Table UTILISATEUR
CREATE TABLE UTILISATEUR (
    id_utilisateur INT AUTO_INCREMENT,
    nom_utilisateur VARCHAR(50) NOT NULL,
    prenom_utilisateur VARCHAR(50) NOT NULL,
    email VARCHAR(100) NOT NULL,
    mot_de_passe VARCHAR(255) NOT NULL,
    role VARCHAR(20) NOT NULL DEFAULT 'Client',
    PRIMARY KEY (id_utilisateur),
    UNIQUE (email)
) ENGINE=InnoDB;

-- 2. Table PASSAGER
CREATE TABLE PASSAGER (
    id_passager INT AUTO_INCREMENT,
    nom_passager VARCHAR(50) NOT NULL,
    prenom_passager VARCHAR(50) NOT NULL,
    date_naissance DATE NOT NULL,
    numero_document VARCHAR(50),
    PRIMARY KEY (id_passager)
) ENGINE=InnoDB;

-- 3. Table VOL
CREATE TABLE VOL (
    id_vol INT AUTO_INCREMENT,
    numero_vol VARCHAR(20) NOT NULL,
    compagnie VARCHAR(50) NOT NULL,
    ville_depart VARCHAR(50) NOT NULL,
    ville_arrivee VARCHAR(50) NOT NULL,
    date_depart DATETIME NOT NULL,
    date_arrivee DATETIME NOT NULL,
    prix DECIMAL(10,2) NOT NULL CHECK (prix > 0),
    places_disponibles INT NOT NULL CHECK (places_disponibles >= 0),
    PRIMARY KEY (id_vol),
    UNIQUE (numero_vol)
) ENGINE=InnoDB;

-- 4. Table RESERVATION
CREATE TABLE RESERVATION (
    id_reservation INT AUTO_INCREMENT,
    reference VARCHAR(20) NOT NULL,
    date_reservation DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    nombre_billets INT NOT NULL CHECK (nombre_billets > 0),
    prix_total DECIMAL(10,2) NOT NULL,
    statut VARCHAR(20) NOT NULL DEFAULT 'Confirmée',
    id_vol INT NOT NULL,
    id_utilisateur INT NOT NULL,
    PRIMARY KEY (id_reservation),
    UNIQUE (reference),
    FOREIGN KEY (id_vol) REFERENCES VOL(id_vol) ON DELETE CASCADE,
    FOREIGN KEY (id_utilisateur) REFERENCES UTILISATEUR(id_utilisateur) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 5. Table pivot CONTIENT_PASSAGER
CREATE TABLE CONTIENT_PASSAGER (
    id_reservation INT NOT NULL,
    id_passager INT NOT NULL,
    PRIMARY KEY (id_reservation, id_passager),
    FOREIGN KEY (id_reservation) REFERENCES RESERVATION(id_reservation) ON DELETE CASCADE,
    FOREIGN KEY (id_passager) REFERENCES PASSAGER(id_passager) ON DELETE CASCADE
) ENGINE=InnoDB;
