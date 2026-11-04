-- ============================================================================
-- Script complet d'initialisation de la base MySQL : reservation_vol_db
-- Projet L1 Informatique - Système de réservation de billets d'avion (C++/Qt6)
-- ============================================================================

CREATE DATABASE IF NOT EXISTS `reservation_vol_db`
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE `reservation_vol_db`;

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- Nettoyage des anciennes tables inutilisées et recréation propre du schéma
DROP TABLE IF EXISTS `notification_message`;
DROP TABLE IF EXISTS `passager`;
DROP TABLE IF EXISTS `reservation_vol`;
DROP TABLE IF EXISTS `reservation`;
DROP TABLE IF EXISTS `siege`;
DROP TABLE IF EXISTS `alerte`;
DROP TABLE IF EXISTS `journal`;
DROP TABLE IF EXISTS `parametre`;
DROP TABLE IF EXISTS `pays_regle_transit`;
DROP TABLE IF EXISTS `programme`;
DROP TABLE IF EXISTS `avion`;
DROP TABLE IF EXISTS `compagnie`;
DROP TABLE IF EXISTS `vol`;
DROP TABLE IF EXISTS `liaison`;
DROP TABLE IF EXISTS `aeroport`;
DROP TABLE IF EXISTS `utilisateur`;

SET FOREIGN_KEY_CHECKS = 1;

-- 1. Table des comptes utilisateurs (clients, admins et super_admin)
CREATE TABLE `utilisateur` (
  `id_utilisateur` INT NOT NULL AUTO_INCREMENT,
  `nom` VARCHAR(50) NOT NULL,
  `prenom` VARCHAR(50) NOT NULL,
  `email` VARCHAR(100) NOT NULL,
  `mot_de_passe` VARCHAR(255) NOT NULL,
  `role` ENUM('client','admin','super_admin') NOT NULL DEFAULT 'client',
  `est_bloque` TINYINT(1) NOT NULL DEFAULT 0,
  `date_creation` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id_utilisateur`),
  UNIQUE KEY `uq_utilisateur_email` (`email`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2. Table de référence des villes, pays et aéroports (plusieurs aéroports possibles par ville)
CREATE TABLE `aeroport` (
  `id_aeroport` INT NOT NULL AUTO_INCREMENT,
  `code_iata` CHAR(3) NOT NULL,
  `nom_aeroport` VARCHAR(100) NOT NULL,
  `ville` VARCHAR(80) NOT NULL,
  `pays` VARCHAR(80) NOT NULL,
  `code_pays` CHAR(2) NOT NULL,
  `latitude` DECIMAL(9,6) NOT NULL,
  `longitude` DECIMAL(9,6) NOT NULL,
  `est_hub` TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id_aeroport`),
  UNIQUE KEY `uq_aeroport_iata` (`code_iata`),
  KEY `idx_aeroport_ville` (`ville`, `pays`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3. Table de référence des liaisons aériennes (distances, durées et tarifs de référence)
CREATE TABLE `liaison` (
  `id_liaison` INT NOT NULL AUTO_INCREMENT,
  `id_aeroport_depart` INT NOT NULL,
  `id_aeroport_arrivee` INT NOT NULL,
  `distance_km` INT NOT NULL,
  `duree_minutes` INT NOT NULL,
  `prix_base` DECIMAL(8,2) NOT NULL,
  PRIMARY KEY (`id_liaison`),
  UNIQUE KEY `uq_liaison_trajet` (`id_aeroport_depart`, `id_aeroport_arrivee`),
  CONSTRAINT `fk_liaison_depart` FOREIGN KEY (`id_aeroport_depart`) REFERENCES `aeroport` (`id_aeroport`) ON DELETE CASCADE,
  CONSTRAINT `fk_liaison_arrivee` FOREIGN KEY (`id_aeroport_arrivee`) REFERENCES `aeroport` (`id_aeroport`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 4. Table des vols (programmés ou générés automatiquement lors d'une réservation)
CREATE TABLE `vol` (
  `id_vol` INT NOT NULL AUTO_INCREMENT,
  `numero_vol` VARCHAR(16) NOT NULL,
  `compagnie` VARCHAR(60) NOT NULL DEFAULT 'Madagascar Airlines',
  `id_aeroport_depart` INT NOT NULL,
  `id_aeroport_arrivee` INT NOT NULL,
  `date_depart` DATETIME NOT NULL,
  `date_arrivee` DATETIME NOT NULL,
  `distance_km` INT NOT NULL,
  `duree_minutes` INT NOT NULL,
  `prix` DECIMAL(8,2) NOT NULL,
  `capacite` INT NOT NULL DEFAULT 120,
  `places_disponibles` INT NOT NULL DEFAULT 120,
  `statut_vol` ENUM('programme','annule') NOT NULL DEFAULT 'programme',
  PRIMARY KEY (`id_vol`),
  UNIQUE KEY `uq_vol_numero` (`numero_vol`),
  KEY `idx_vol_recherche` (`id_aeroport_depart`, `id_aeroport_arrivee`, `date_depart`),
  CONSTRAINT `fk_vol_depart` FOREIGN KEY (`id_aeroport_depart`) REFERENCES `aeroport` (`id_aeroport`),
  CONSTRAINT `fk_vol_arrivee` FOREIGN KEY (`id_aeroport_arrivee`) REFERENCES `aeroport` (`id_aeroport`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 5. Table des réservations (statut initial 'en_attente' jusqu'à validation admin)
CREATE TABLE `reservation` (
  `id_reservation` INT NOT NULL AUTO_INCREMENT,
  `reference` VARCHAR(20) NOT NULL,
  `id_utilisateur` INT NOT NULL,
  `nb_places` INT NOT NULL,
  `prix_total` DECIMAL(10,2) NOT NULL,
  `statut` ENUM('en_attente','confirmee','annulee') NOT NULL DEFAULT 'en_attente',
  `nb_escales` INT NOT NULL DEFAULT 0,
  `mode_paiement` VARCHAR(40) NOT NULL DEFAULT '',
  `contact_paiement` VARCHAR(100) NOT NULL DEFAULT '',
  `reference_paiement` VARCHAR(80) NOT NULL DEFAULT '',
  `date_reservation` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id_reservation`),
  UNIQUE KEY `uq_reservation_reference` (`reference`),
  KEY `idx_reservation_utilisateur` (`id_utilisateur`),
  KEY `idx_reservation_statut` (`statut`),
  CONSTRAINT `fk_reservation_utilisateur` FOREIGN KEY (`id_utilisateur`) REFERENCES `utilisateur` (`id_utilisateur`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 6. Table d'association ordonnée entre une réservation et ses 1 à 3 vols (vol direct ou avec 1 à 2 escales)
CREATE TABLE `reservation_vol` (
  `id_reservation` INT NOT NULL,
  `ordre_troncon` INT NOT NULL,
  `id_vol` INT NOT NULL,
  PRIMARY KEY (`id_reservation`, `ordre_troncon`),
  KEY `idx_resvol_vol` (`id_vol`),
  CONSTRAINT `fk_resvol_reservation` FOREIGN KEY (`id_reservation`) REFERENCES `reservation` (`id_reservation`) ON DELETE CASCADE,
  CONSTRAINT `fk_resvol_vol` FOREIGN KEY (`id_vol`) REFERENCES `vol` (`id_vol`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 7. Table des passagers rattachés à chaque réservation (avec numéro de passeport vérifié)
CREATE TABLE `passager` (
  `id_passager` INT NOT NULL AUTO_INCREMENT,
  `id_reservation` INT NOT NULL,
  `nom` VARCHAR(60) NOT NULL,
  `prenom` VARCHAR(60) NOT NULL,
  `date_naissance` DATE NOT NULL,
  `numero_passeport` VARCHAR(20) NOT NULL,
  `siege` VARCHAR(8) NOT NULL,
  PRIMARY KEY (`id_passager`),
  KEY `idx_passager_reservation` (`id_reservation`),
  CONSTRAINT `fk_passager_reservation` FOREIGN KEY (`id_reservation`) REFERENCES `reservation` (`id_reservation`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 8. Table de boîte de réception et notifications multi-rôles (clients, admins, super_admin)
CREATE TABLE `notification_message` (
  `id_message` INT NOT NULL AUTO_INCREMENT,
  `id_destinataire` INT NOT NULL DEFAULT 0,
  `role_cible` ENUM('client','admin') NOT NULL DEFAULT 'client',
  `id_expediteur` INT NOT NULL DEFAULT 0,
  `nom_expediteur` VARCHAR(100) NOT NULL,
  `email_expediteur` VARCHAR(100) NOT NULL DEFAULT '',
  `categorie` VARCHAR(40) NOT NULL DEFAULT 'Système',
  `sujet` VARCHAR(160) NOT NULL,
  `contenu` TEXT NOT NULL,
  `est_lu` TINYINT(1) NOT NULL DEFAULT 0,
  `date_envoi` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id_message`),
  KEY `idx_notif_dest` (`id_destinataire`, `role_cible`, `est_lu`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Comptes initiaux :
-- Super Admin : superadmin@billets-avion.mg / SuperAdmin2026!
-- Admin       : admin@billets-avion.mg      / Admin2026!
-- Client démo : client@billets-avion.mg     / Client2026!
INSERT INTO `utilisateur` (`id_utilisateur`, `nom`, `prenom`, `email`, `mot_de_passe`, `role`) VALUES
  (1, 'RAKOTO', 'Tahina', 'superadmin@billets-avion.mg', 'a1b2c3d4e5f60718293a4b5c6d7e8f90$aa997d0469becf25e50f83f7b6122d69b2e4bcf492cbfd5193fffee04707ae12', 'super_admin'),
  (2, 'RANDRIA', 'Koto', 'admin@billets-avion.mg', '11223344556677889900aabbccddeeff$f2938ff57b20a389780e7aafb4925d86814a9d8db531feb081e463c64964834d', 'admin'),
  (3, 'RASOA', 'Miora', 'client@billets-avion.mg', 'ffeeddccbbaa00998877665544332211$d45f893fcf0aa8bc832e1c7d815a1ac981b3c670c0eff24891c67a99b5a7518d', 'client');

-- Données de référence des villes et aéroports
INSERT INTO `aeroport` (`id_aeroport`, `code_iata`, `nom_aeroport`, `ville`, `pays`, `code_pays`, `latitude`, `longitude`, `est_hub`) VALUES
  (1, 'TNR', 'Aéroport International Ivato', 'Antananarivo', 'Madagascar', 'MG', -18.796900, 47.478800, 1),
  (2, 'WFI', 'Aéroport de Fianarantsoa-Beravina', 'Fianarantsoa', 'Madagascar', 'MG', -21.441700, 47.111700, 0),
  (3, 'NOS', 'Aéroport International de Fascene', 'Nosy Be', 'Madagascar', 'MG', -13.312100, 48.314800, 1),
  (4, 'TMM', 'Aéroport de Toamasina-Ambalamanasy', 'Toamasina', 'Madagascar', 'MG', -18.109500, 49.392500, 0),
  (5, 'MJN', 'Aéroport Philibert Tsiranana', 'Mahajanga', 'Madagascar', 'MG', -15.667100, 46.351800, 0),
  (6, 'TLE', 'Aéroport de Toliara', 'Toliara', 'Madagascar', 'MG', -23.383400, 43.728500, 0),
  (7, 'DIE', 'Aéroport dArrachart', 'Antsiranana', 'Madagascar', 'MG', -12.349400, 49.291700, 0),
  (8, 'SMS', 'Aéroport de Sainte-Marie', 'Sainte-Marie', 'Madagascar', 'MG', -17.093900, 49.815800, 0),
  (9, 'FTU', 'Aéroport de Taolagnaro-Marillac', 'Taolagnaro', 'Madagascar', 'MG', -25.038100, 46.956200, 0),
  (10, 'RUN', 'Aéroport de La Réunion Roland-Garros', 'Saint-Denis', 'La Réunion', 'RE', -20.887100, 55.510300, 1),
  (11, 'ZSE', 'Aéroport de Pierrefonds', 'Saint-Denis', 'La Réunion', 'RE', -21.320400, 55.424900, 0),
  (12, 'MRU', 'Aéroport Sir Seewoosagur Ramgoolam', 'Port-Louis', 'Maurice', 'MU', -20.430200, 57.683600, 1),
  (13, 'HAH', 'Aéroport Prince Saïd Ibrahim', 'Moroni', 'Comores', 'KM', -11.533700, 43.271900, 0),
  (14, 'DZA', 'Aéroport de Dzaoudzi-Pamandzi', 'Dzaoudzi', 'Mayotte', 'YT', -12.804700, 45.281100, 0),
  (15, 'SEZ', 'Aéroport International des Seychelles', 'Mahé', 'Seychelles', 'SC', -4.674300, 55.521800, 0),
  (16, 'NBO', 'Aéroport Jomo Kenyatta', 'Nairobi', 'Kenya', 'KE', -1.319200, 36.927800, 1),
  (17, 'ADD', 'Aéroport International de Bole', 'Addis-Abeba', 'Éthiopie', 'ET', 8.977900, 38.799300, 1),
  (18, 'JNB', 'Aéroport O. R. Tambo', 'Johannesburg', 'Afrique du Sud', 'ZA', -26.139200, 28.246000, 1),
  (19, 'CAI', 'Aéroport International du Caire', 'Le Caire', 'Égypte', 'EG', 30.121900, 31.405600, 1),
  (20, 'CMN', 'Aéroport Mohammed V', 'Casablanca', 'Maroc', 'MA', 33.367500, -7.589970, 1),
  (21, 'DSS', 'Aéroport Blaise Diagne', 'Dakar', 'Sénégal', 'SN', 14.670000, -17.073300, 0),
  (22, 'DXB', 'Aéroport International de Dubaï', 'Dubaï', 'Émirats arabes unis', 'AE', 25.253200, 55.365700, 1),
  (23, 'DOH', 'Aéroport International Hamad', 'Doha', 'Qatar', 'QA', 25.273100, 51.608100, 1),
  (24, 'IST', 'Aéroport dIstanbul', 'Istanbul', 'Turquie', 'TR', 41.275300, 28.751900, 1),
  (25, 'SAW', 'Aéroport Sabiha Gökçen', 'Istanbul', 'Turquie', 'TR', 40.898600, 29.309200, 0),
  (26, 'CDG', 'Aéroport Paris-Charles de Gaulle', 'Paris', 'France', 'FR', 49.009700, 2.547900, 1),
  (27, 'ORY', 'Aéroport de Paris-Orly', 'Paris', 'France', 'FR', 48.726200, 2.365200, 1),
  (28, 'LYS', 'Aéroport de Lyon-Saint-Exupéry', 'Lyon', 'France', 'FR', 45.725600, 5.081100, 0),
  (29, 'MRS', 'Aéroport Marseille Provence', 'Marseille', 'France', 'FR', 43.439300, 5.221400, 0),
  (30, 'NCE', 'Aéroport Nice Côte dAzur', 'Nice', 'France', 'FR', 43.658400, 7.215900, 0),
  (31, 'LHR', 'Aéroport de Londres-Heathrow', 'Londres', 'Royaume-Uni', 'GB', 51.470000, -0.454300, 1),
  (32, 'LGW', 'Aéroport de Londres-Gatwick', 'Londres', 'Royaume-Uni', 'GB', 51.153700, -0.182100, 0),
  (33, 'FRA', 'Aéroport de Francfort-sur-le-Main', 'Francfort', 'Allemagne', 'DE', 50.037900, 8.562200, 1),
  (34, 'MAD', 'Aéroport Adolfo-Suárez de Madrid-Barajas', 'Madrid', 'Espagne', 'ES', 40.498300, -3.567600, 1),
  (35, 'FCO', 'Aéroport Léonard-de-Vinci Fiumicino', 'Rome', 'Italie', 'IT', 41.800300, 12.238900, 1),
  (36, 'MXP', 'Aéroport de Milan-Malpensa', 'Milan', 'Italie', 'IT', 45.630600, 8.728100, 0),
  (37, 'LIN', 'Aéroport de Milan-Linate', 'Milan', 'Italie', 'IT', 45.445100, 9.276700, 0),
  (38, 'BRU', 'Aéroport de Bruxelles-National', 'Bruxelles', 'Belgique', 'BE', 50.901400, 4.484400, 0),
  (39, 'GVA', 'Aéroport International de Genève', 'Genève', 'Suisse', 'CH', 46.238100, 6.108900, 0),
  (40, 'AMS', 'Aéroport dAmsterdam-Schiphol', 'Amsterdam', 'Pays-Bas', 'NL', 52.310500, 4.768300, 1),
  (41, 'JFK', 'Aéroport John F. Kennedy', 'New York', 'États-Unis', 'US', 40.641300, -73.778100, 1),
  (42, 'EWR', 'Aéroport Newark Liberty', 'New York', 'États-Unis', 'US', 40.689500, -74.174500, 0),
  (43, 'YUL', 'Aéroport Montréal-Trudeau', 'Montréal', 'Canada', 'CA', 45.470600, -73.740800, 0),
  (44, 'SIN', 'Aéroport de Singapour-Changi', 'Singapour', 'Singapour', 'SG', 1.364400, 103.991500, 1),
  (45, 'BKK', 'Aéroport de Bangkok-Suvarnabhumi', 'Bangkok', 'Thaïlande', 'TH', 13.690000, 100.750100, 1),
  (46, 'HND', 'Aéroport de Tokyo-Haneda', 'Tokyo', 'Japon', 'JP', 35.549400, 139.779800, 1),
  (47, 'NRT', 'Aéroport de Tokyo-Narita', 'Tokyo', 'Japon', 'JP', 35.772000, 140.392900, 0),
  (48, 'SYD', 'Aéroport de Sydney-Kingsford Smith', 'Sydney', 'Australie', 'AU', -33.939900, 151.175300, 1);

-- Liaisons aériennes de référence (distances, durées et tarifs cohérents)
INSERT INTO `liaison` (`id_liaison`, `id_aeroport_depart`, `id_aeroport_arrivee`, `distance_km`, `duree_minutes`, `prix_base`) VALUES
  (1, 1, 2, 297, 52, 60.25),
  (2, 2, 1, 297, 52, 60.25),
  (3, 1, 3, 616, 76, 87.36),
  (4, 3, 1, 616, 76, 87.36),
  (5, 1, 4, 216, 46, 53.36),
  (6, 4, 1, 216, 46, 53.36),
  (7, 1, 5, 368, 58, 66.28),
  (8, 5, 1, 368, 58, 66.28),
  (9, 1, 6, 641, 78, 89.49),
  (10, 6, 1, 641, 78, 89.49),
  (11, 1, 7, 743, 86, 98.16),
  (12, 7, 1, 743, 86, 98.16),
  (13, 1, 8, 311, 53, 61.44),
  (14, 8, 1, 311, 53, 61.44),
  (15, 1, 9, 696, 82, 94.16),
  (16, 9, 1, 696, 82, 94.16),
  (17, 3, 7, 151, 45, 47.84),
  (18, 7, 3, 151, 45, 47.84),
  (19, 3, 5, 336, 55, 63.56),
  (20, 5, 3, 336, 55, 63.56),
  (21, 2, 6, 409, 61, 69.77),
  (22, 6, 2, 409, 61, 69.77),
  (23, 2, 9, 400, 60, 69.00),
  (24, 9, 2, 400, 60, 69.00),
  (25, 1, 10, 871, 95, 109.04),
  (26, 10, 1, 871, 95, 109.04),
  (27, 1, 12, 1084, 111, 127.14),
  (28, 12, 1, 1084, 111, 127.14),
  (29, 1, 13, 925, 99, 113.62),
  (30, 13, 1, 925, 99, 113.62),
  (31, 1, 14, 707, 83, 95.09),
  (32, 14, 1, 707, 83, 95.09),
  (33, 1, 15, 1797, 165, 187.75),
  (34, 15, 1, 1797, 165, 187.75),
  (35, 1, 16, 2258, 199, 226.93),
  (36, 16, 1, 2258, 199, 226.93),
  (37, 1, 17, 3232, 272, 309.72),
  (38, 17, 1, 3232, 272, 309.72),
  (39, 1, 18, 2135, 190, 216.48),
  (40, 18, 1, 2135, 190, 216.48),
  (41, 3, 10, 1137, 115, 131.65),
  (42, 10, 3, 1137, 115, 131.65),
  (43, 3, 14, 333, 55, 63.31),
  (44, 14, 3, 333, 55, 63.31),
  (45, 3, 17, 2692, 232, 263.82),
  (46, 17, 3, 2692, 232, 263.82),
  (47, 10, 12, 232, 47, 54.72),
  (48, 12, 10, 232, 47, 54.72),
  (49, 10, 11, 49, 45, 39.16),
  (50, 11, 10, 49, 45, 39.16),
  (51, 1, 26, 8748, 686, 778.58),
  (52, 26, 1, 8748, 686, 778.58),
  (53, 1, 27, 8734, 685, 777.39),
  (54, 27, 1, 8734, 685, 777.39),
  (55, 1, 24, 6953, 551, 626.00),
  (56, 24, 1, 6953, 551, 626.00),
  (57, 10, 27, 9358, 732, 830.43),
  (58, 27, 10, 9358, 732, 830.43),
  (59, 10, 26, 9369, 733, 831.37),
  (60, 26, 10, 9369, 733, 831.37),
  (61, 10, 29, 8801, 690, 783.09),
  (62, 29, 10, 8801, 690, 783.09),
  (63, 12, 26, 9447, 739, 838.00),
  (64, 26, 12, 9447, 739, 838.00),
  (65, 12, 32, 9748, 761, 863.58),
  (66, 32, 12, 9748, 761, 863.58),
  (67, 12, 22, 5086, 411, 467.31),
  (68, 22, 12, 5086, 411, 467.31),
  (69, 12, 24, 7482, 591, 670.97),
  (70, 24, 12, 7482, 591, 670.97),
  (71, 17, 26, 5581, 449, 509.39),
  (72, 26, 17, 5581, 449, 509.39),
  (73, 17, 33, 5350, 431, 489.75),
  (74, 33, 17, 5350, 431, 489.75),
  (75, 17, 31, 5919, 474, 538.12),
  (76, 31, 17, 5919, 474, 538.12),
  (77, 17, 22, 2519, 219, 249.12),
  (78, 22, 17, 2519, 219, 249.12),
  (79, 17, 19, 2474, 216, 245.29),
  (80, 19, 17, 2474, 216, 245.29),
  (81, 17, 41, 11200, 870, 987.00),
  (82, 41, 17, 11200, 870, 987.00),
  (83, 16, 26, 6495, 517, 587.08),
  (84, 26, 16, 6495, 517, 587.08),
  (85, 16, 31, 6841, 543, 616.49),
  (86, 31, 16, 6841, 543, 616.49),
  (87, 16, 40, 6677, 531, 602.55),
  (88, 40, 16, 6677, 531, 602.55),
  (89, 16, 22, 3559, 297, 337.52),
  (90, 22, 16, 3559, 297, 337.52),
  (91, 16, 23, 3353, 281, 320.00),
  (92, 23, 16, 3353, 281, 320.00),
  (93, 18, 31, 9075, 711, 806.38),
  (94, 31, 18, 9075, 711, 806.38),
  (95, 18, 26, 8737, 685, 777.65),
  (96, 26, 18, 8737, 685, 777.65),
  (97, 18, 22, 6413, 511, 580.11),
  (98, 22, 18, 6413, 511, 580.11),
  (99, 19, 26, 3210, 271, 307.85),
  (100, 26, 19, 3210, 271, 307.85),
  (101, 19, 35, 2151, 191, 217.84),
  (102, 35, 19, 2151, 191, 217.84),
  (103, 19, 22, 2416, 211, 240.36),
  (104, 22, 19, 2416, 211, 240.36),
  (105, 20, 26, 1931, 175, 199.14),
  (106, 26, 20, 1931, 175, 199.14),
  (107, 20, 27, 1897, 172, 196.25),
  (108, 27, 20, 1897, 172, 196.25),
  (109, 20, 34, 870, 95, 108.95),
  (110, 34, 20, 870, 95, 108.95),
  (111, 20, 21, 2288, 202, 229.48),
  (112, 21, 20, 2288, 202, 229.48),
  (113, 20, 43, 5674, 456, 517.29),
  (114, 43, 20, 5674, 456, 517.29),
  (115, 22, 26, 5239, 423, 480.32),
  (116, 26, 22, 5239, 423, 480.32),
  (117, 22, 31, 5497, 442, 502.25),
  (118, 31, 22, 5497, 442, 502.25),
  (119, 22, 33, 4844, 393, 446.74),
  (120, 33, 22, 4844, 393, 446.74),
  (121, 22, 44, 5845, 468, 531.83),
  (122, 44, 22, 5845, 468, 531.83),
  (123, 22, 45, 4904, 398, 451.84),
  (124, 45, 22, 4904, 398, 451.84),
  (125, 22, 46, 7935, 625, 709.48),
  (126, 46, 22, 7935, 625, 709.48),
  (127, 22, 47, 7979, 628, 713.22),
  (128, 47, 22, 7979, 628, 713.22),
  (129, 22, 41, 11001, 855, 970.09),
  (130, 41, 22, 11001, 855, 970.09),
  (131, 22, 48, 12043, 933, 1058.66),
  (132, 48, 22, 12043, 933, 1058.66),
  (133, 23, 26, 4969, 403, 457.37),
  (134, 26, 23, 4969, 403, 457.37),
  (135, 23, 31, 5241, 423, 480.49),
  (136, 31, 23, 5241, 423, 480.49),
  (137, 23, 44, 6206, 495, 562.51),
  (138, 44, 23, 6206, 495, 562.51),
  (139, 24, 26, 2214, 196, 223.19),
  (140, 26, 24, 2214, 196, 223.19),
  (141, 24, 33, 1838, 168, 191.23),
  (142, 33, 24, 1838, 168, 191.23),
  (143, 24, 31, 2488, 217, 246.48),
  (144, 31, 24, 2488, 217, 246.48),
  (145, 24, 41, 8027, 632, 717.30),
  (146, 41, 24, 8027, 632, 717.30),
  (147, 26, 28, 412, 61, 70.02),
  (148, 28, 26, 412, 61, 70.02),
  (149, 26, 29, 653, 79, 90.50),
  (150, 29, 26, 653, 79, 90.50),
  (151, 26, 30, 694, 82, 93.99),
  (152, 30, 26, 694, 82, 93.99),
  (153, 27, 29, 628, 77, 88.38),
  (154, 29, 27, 628, 77, 88.38),
  (155, 27, 30, 676, 81, 92.46),
  (156, 30, 27, 676, 81, 92.46),
  (157, 27, 28, 392, 59, 68.32),
  (158, 28, 27, 392, 59, 68.32),
  (159, 26, 31, 347, 56, 64.50),
  (160, 31, 26, 347, 56, 64.50),
  (161, 26, 33, 449, 64, 73.17),
  (162, 33, 26, 449, 64, 73.17),
  (163, 26, 34, 1062, 110, 125.27),
  (164, 34, 26, 1062, 110, 125.27),
  (165, 26, 35, 1101, 113, 128.59),
  (166, 35, 26, 1101, 113, 128.59),
  (167, 26, 36, 598, 75, 85.83),
  (168, 36, 26, 598, 75, 85.83),
  (169, 26, 37, 644, 78, 89.74),
  (170, 37, 26, 644, 78, 89.74),
  (171, 26, 38, 252, 49, 56.42),
  (172, 38, 26, 252, 49, 56.42),
  (173, 26, 39, 408, 61, 69.68),
  (174, 39, 26, 408, 61, 69.68),
  (175, 26, 40, 399, 60, 68.91),
  (176, 40, 26, 399, 60, 68.91),
  (177, 26, 41, 5833, 467, 530.81),
  (178, 41, 26, 5833, 467, 530.81),
  (179, 26, 42, 5857, 469, 532.85),
  (180, 42, 26, 5857, 469, 532.85),
  (181, 26, 43, 5523, 444, 504.46),
  (182, 43, 26, 5523, 444, 504.46),
  (183, 26, 46, 9707, 758, 860.10),
  (184, 46, 26, 9707, 758, 860.10),
  (185, 26, 44, 10723, 834, 946.46),
  (186, 44, 26, 10723, 834, 946.46),
  (187, 31, 41, 5540, 446, 505.90),
  (188, 41, 31, 5540, 446, 505.90),
  (189, 31, 42, 5563, 447, 507.86),
  (190, 42, 31, 5563, 447, 507.86),
  (191, 44, 48, 6294, 502, 569.99),
  (192, 48, 44, 6294, 502, 569.99),
  (193, 44, 46, 5299, 427, 485.42),
  (194, 46, 44, 5299, 427, 485.42),
  (195, 44, 45, 1416, 136, 155.36),
  (196, 45, 44, 1416, 136, 155.36);

-- Exemples de vols programmés dans le futur
INSERT INTO `vol` (`id_vol`, `numero_vol`, `compagnie`, `id_aeroport_depart`, `id_aeroport_arrivee`, `date_depart`, `date_arrivee`, `distance_km`, `duree_minutes`, `prix`, `capacite`, `places_disponibles`, `statut_vol`) VALUES
  (1, 'MD-101', 'Madagascar Airlines', 1, 2, '2026-11-05 08:00:00', '2026-11-05 08:52:00', 297, 52, 60.25, 70, 70, 'programme'),
  (2, 'MD-102', 'Madagascar Airlines', 2, 1, '2026-11-05 10:30:00', '2026-11-05 11:22:00', 297, 52, 60.25, 70, 70, 'programme'),
  (3, 'MD-204', 'Madagascar Airlines', 1, 3, '2026-11-06 07:30:00', '2026-11-06 08:46:00', 616, 76, 87.36, 90, 90, 'programme'),
  (4, 'MD-205', 'Madagascar Airlines', 3, 1, '2026-11-06 11:00:00', '2026-11-06 12:16:00', 616, 76, 87.36, 90, 90, 'programme'),
  (5, 'MD-310', 'Madagascar Airlines', 1, 4, '2026-11-07 09:00:00', '2026-11-07 09:46:00', 216, 46, 53.36, 80, 80, 'programme'),
  (6, 'MD-412', 'Madagascar Airlines', 1, 6, '2026-11-08 08:30:00', '2026-11-08 09:48:00', 641, 78, 89.49, 80, 80, 'programme'),
  (7, 'UU-612', 'Air Austral', 1, 10, '2026-11-10 09:00:00', '2026-11-10 10:35:00', 871, 95, 109.04, 120, 120, 'programme'),
  (8, 'UU-975', 'Air Austral', 10, 26, '2026-11-10 13:30:00', '2026-11-11 01:43:00', 9369, 733, 831.37, 180, 180, 'programme'),
  (9, 'AF-935', 'Air France', 1, 26, '2026-11-10 22:30:00', '2026-11-11 09:56:00', 8748, 686, 778.58, 220, 220, 'programme'),
  (10, 'AF-934', 'Air France', 26, 1, '2026-11-12 10:40:00', '2026-11-12 22:06:00', 8748, 686, 778.58, 220, 220, 'programme'),
  (11, 'ET-852', 'Ethiopian Airlines', 1, 17, '2026-11-11 14:20:00', '2026-11-11 18:52:00', 3232, 272, 309.72, 160, 160, 'programme'),
  (12, 'ET-704', 'Ethiopian Airlines', 17, 26, '2026-11-11 23:05:00', '2026-11-12 06:34:00', 5581, 449, 509.39, 200, 200, 'programme'),
  (13, 'MK-289', 'Air Mauritius', 1, 12, '2026-11-14 11:15:00', '2026-11-14 13:06:00', 1084, 111, 127.14, 140, 140, 'programme'),
  (14, 'EK-702', 'Emirates', 12, 22, '2026-11-14 16:30:00', '2026-11-14 23:21:00', 5086, 411, 467.31, 250, 250, 'programme'),
  (15, 'EK-073', 'Emirates', 22, 26, '2026-11-15 02:00:00', '2026-11-15 09:03:00', 5239, 423, 480.32, 250, 250, 'programme'),
  (16, 'AF-610', 'Air France', 27, 29, '2026-11-15 08:15:00', '2026-11-15 09:32:00', 628, 77, 88.38, 150, 150, 'programme'),
  (17, 'AF-006', 'Air France', 26, 41, '2026-11-16 13:30:00', '2026-11-16 21:17:00', 5833, 467, 530.81, 240, 240, 'programme'),
  (18, 'KQ-257', 'Kenya Airways', 1, 16, '2026-11-18 07:45:00', '2026-11-18 11:04:00', 2258, 199, 226.93, 130, 130, 'programme'),
  (19, 'KQ-112', 'Kenya Airways', 16, 26, '2026-11-18 13:30:00', '2026-11-18 22:07:00', 6495, 517, 587.08, 190, 190, 'programme'),
  (20, 'MD-115', 'Madagascar Airlines', 2, 1, '2026-11-11 08:00:00', '2026-11-11 08:52:00', 297, 52, 60.25, 70, 70, 'programme');

-- Messages de bienvenue initiaux dans la boîte de réception
INSERT INTO `notification_message` (`id_destinataire`, `role_cible`, `id_expediteur`, `nom_expediteur`, `email_expediteur`, `categorie`, `sujet`, `contenu`) VALUES
  (3, 'client', 0, 'Système Billets d''Avion', 'noreply@billets-avion.mg', 'Système', 'Bienvenue dans votre espace voyageur', 'Vous recevrez ici le suivi en temps réel de vos réservations, vos confirmations de billets et les réponses de notre équipe d''assistance.'),
  (0, 'admin', 0, 'Système Billets d''Avion', 'noreply@billets-avion.mg', 'Système', 'Centre de notifications Administrateur actif', 'Les nouvelles réservations clients, demandes d''assistance (mots de passe oubliés, comptes bloqués) et signalements apparaîtront directement dans cette boîte de réception.');
