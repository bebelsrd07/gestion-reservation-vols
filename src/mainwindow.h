#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "modeles.h"
#include <QHash>
#include <QMainWindow>
#include <QPair>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class DatabaseManager;
class PlanAvionWidget;
class QAction;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPaintEvent;
class QPushButton;
class QResizeEvent;
class QSpinBox;
class QSqlQueryModel;
class QStandardItem;
class QStandardItemModel;
class QTableView;
class QTabWidget;
class QTimer;
class QVBoxLayout;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DatabaseManager *db, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // Recherche, monnaie globale, défilement auto et réinitialisation
    void changerMonnaieGlobale();
    void actualiserAeroportsDepart();
    void actualiserAeroportsArrivee();
    void mettreAJourApercuReference();
    void lancerRecherche();
    void reinitialiserFiltres();
    void ouvrirFormulaireReservation();
    void faireDefilerTableauVols();
    void actualiserVueActive();

    // Authentification
    void traiterConnexion();
    void traiterInscription();
    void passerEnModeVisiteur();
    void seDeconnecter();

    // Réservation multi-passagers, plan d'avion, paiement TicketFly et passeports sans OCR
    void mettreAJourSiegesEtTarifReservation();
    void actualiserInstructionsPaiement();
    void importerPasseportPassager(int indexPassager);
    void verifierConcordancePassager(int indexPassager);
    void validerDemandeReservation();

    // Espace client "Mes réservations"
    void actualiserMesReservations();
    void afficherDetailMaReservation();
    void telechargerMonBilletPdf();
    void annulerMaReservation();

    // Assistance globale et boîte de réception simplifiée
    void ouvrirDialogueAssistance(const QString &sujetPreselectionne = QString());
    void actualiserBoiteReception();
    void lireMessageSelectionne();
    void repondreAuClientDepuisBoite();

    // Espace Administration & Super Administration
    void actualiserEspaceAdmin();
    void afficherDetailReservationAdmin();
    void appliquerStatutAdmin(const QString &nouveauStatut);
    void exporterCsvAdmin();
    void enregistrerBilletPdfAdmin();
    void chargerVolSelectionneAdmin();
    void ajouterVolAdmin();
    void modifierVolAdmin();
    void supprimerVolAdmin();
    void sauvegarderConfigSmtpAdmin();

    // Gestion & Assistance des clients par Admin / SuperAdmin
    void actualiserListeClientsAdmin();
    void chargerClientSelectionneAdmin();
    void modifierClientDepuisAdmin();
    void supprimerClientDepuisAdmin();
    // CRUD complet des administrateurs par SuperAdmin
    void chargerAdminSelectionneSuperAdmin();
    void creerCompteAdminParSuperAdmin();
    void modifierCompteAdminParSuperAdmin();
    void supprimerCompteAdminParSuperAdmin();

    // Synchronisation automatique multi-instances
    void sonderBaseDeDonnees();

private:
    enum IndexPage {
        PageRecherche = 0,
        PageAcces = 1,
        PageReservation = 2,
        PageMesReservations = 3,
        PageAdmin = 4,
        PageBoiteReception = 5,
        PageAccueil = 6
    };

    struct BlocPassagerUI {
        QLineEdit *editNom = nullptr;
        QLineEdit *editPrenom = nullptr;
        QDateEdit *dateNaissance = nullptr;
        QLineEdit *editNumeroPasseport = nullptr;
        QLabel *lblSiegeAttribue = nullptr;
        QLabel *lblStatutVerification = nullptr;
        bool fichierCharge = false;
        QString numeroLuDansFichier;
        QString nomLuDansFichier;
        QString prenomLuDansFichier;
        bool estVerifie = false;
    };

    void initialiserBarreOutils();
    void initialiserRechercheEtAutocompletion();
    void construirePageAccueil();
    void construirePageAcces();
    void construirePageReservation();
    void construirePageMesReservations();
    void construirePageAdmin();
    void construirePageBoiteReception();
    QString prixFormate(double montantEur) const;

    void executerRechercheOuAffichageComplet(bool signalerErreursSaisie);
    void remplirTableauItineraires();
    QList<QStandardItem *> construireLigneVol(const ItinerairePropose &it) const;
    void rechargerDonneesReference();
    void installerAutocompletionVille(QLineEdit *champ);
    void actualiserComboAeroportVille(const QLineEdit *champVille, QComboBox *combo);
    QList<AeroportInfo> resoudreAeroportsSelectionnes(const QLineEdit *champVille,
                                                      const QComboBox *comboAeroport) const;
    void mettreAJourEtatSessionUI();
    void mettreAJourPastilleBoiteReception();

    Ui::MainWindow *ui = nullptr;
    DatabaseManager *m_db = nullptr;

    // Session courante
    int m_idUtilisateur = 0;
    QString m_role = QStringLiteral("visiteur");
    QString m_nomUtilisateur = QStringLiteral("Visiteur");
    QString m_emailUtilisateur;

    // Données de référence
    QList<AeroportInfo> m_aeroports;
    QStringList m_villesFormatees;
    QHash<QPair<int, int>, LiaisonReference> m_liaisons;

    // Résultats de vols affichés
    QList<ItinerairePropose> m_itinerairesTrouves;
    QStandardItemModel *m_modeleRecherche = nullptr;
    ItinerairePropose m_itineraireEnCours;
    bool m_blocageSignauxFiltres = false;
    QTimer *m_minuteurDefilementVols = nullptr;
    int m_compteurTicksDefilement = 0;

    // Navigation, monnaie & synchronisation
    QAction *m_actAccueil = nullptr;
    QAction *m_actRecherche = nullptr;
    QAction *m_actBoiteReception = nullptr;
    QAction *m_actMesReservations = nullptr;
    QAction *m_actAdmin = nullptr;
    QAction *m_actAssistance = nullptr;
    QAction *m_actSession = nullptr;
    QComboBox *m_comboMonnaie = nullptr;
    QString m_codeMonnaie = QStringLiteral("EUR");
    double m_tauxMonnaiePrecedent = 1.0;
    QPushButton *m_btnSecondaireAccueil = nullptr;
    QWidget *m_decoGaucheVols = nullptr;
    QLabel *m_lblUtilisateurConnecte = nullptr;
    QTimer *m_minuteurSondage = nullptr;
    int m_dernierIdAttenteConnu = -1;
    int m_dernierNbNonLusConnu = 0;
    QString m_derniereEmpreinteClient;

    // Page Accès
    QTabWidget *m_ongletsAcces = nullptr;
    QLineEdit *m_loginEmail = nullptr;
    QLineEdit *m_loginMdp = nullptr;
    QLineEdit *m_inscrNom = nullptr;
    QLineEdit *m_inscrPrenom = nullptr;
    QLineEdit *m_inscrEmail = nullptr;
    QLineEdit *m_inscrMdp = nullptr;
    QLineEdit *m_inscrMdpConfirm = nullptr;

    // Page Réservation + Seatmap + Paiement TicketFly
    QLabel *m_lblResumeReservation = nullptr;
    QWidget *m_conteneurPassagers = nullptr;
    QVBoxLayout *m_layoutPassagers = nullptr;
    QList<BlocPassagerUI> m_blocsPassagers;
    PlanAvionWidget *m_planAvion = nullptr;
    QLabel *m_lblLegendePlan = nullptr;
    QLabel *m_lblTarifSieges = nullptr;
    QLabel *m_lblInstructionsPaiement = nullptr;
    QComboBox *m_comboModePaiement = nullptr;
    QLineEdit *m_editContactPaiement = nullptr;
    QLineEdit *m_editReferencePaiement = nullptr;

    // Page Mes Réservations
    QTableView *m_tableMesReservations = nullptr;
    QSqlQueryModel *m_modeleMesReservations = nullptr;
    QLabel *m_lblDetailMaReservation = nullptr;
    QPushButton *m_btnTelechargerPdfClient = nullptr;

    // Page Boîte de réception simplifiée
    QCheckBox *m_chkSeulementNonLus = nullptr;
    QListWidget *m_listeMessagesBoite = nullptr;
    QLabel *m_lblLectureMessage = nullptr;
    QWidget *m_blocReponseAdmin = nullptr;
    QLineEdit *m_editReponseAdmin = nullptr;
    QList<MessageBoiteReception> m_messagesBoite;

    // Page Administration
    QTabWidget *m_ongletsAdmin = nullptr;
    QWidget *m_ongletSuperAdminWidget = nullptr;
    int m_indexOngletSuperAdmin = -1;
    QLabel *m_lblStatsResume = nullptr;
    QWidget *m_graphiqueStatuts = nullptr;
    QComboBox *m_filtreStatutAdmin = nullptr;
    QTableView *m_tableResAdmin = nullptr;
    QSqlQueryModel *m_modeleResAdmin = nullptr;
    QLabel *m_lblDetailResAdmin = nullptr;

    QTableView *m_tableVolsAdmin = nullptr;
    QSqlQueryModel *m_modeleVolsAdmin = nullptr;
    QComboBox *m_comboVolDepAdmin = nullptr;
    QComboBox *m_comboVolArrAdmin = nullptr;
    QLineEdit *m_editCompagnieAdmin = nullptr;
    QDateTimeEdit *m_dateHeureVolAdmin = nullptr;
    QSpinBox *m_spinCapaciteAdmin = nullptr;
    QDoubleSpinBox *m_spinPrixAdmin = nullptr;
    QCheckBox *m_chkNotifierModifVol = nullptr;

    QLineEdit *m_rechercheClientAdmin = nullptr;
    QTableView *m_tableClientsAdmin = nullptr;
    QSqlQueryModel *m_modeleClientsAdmin = nullptr;
    QLineEdit *m_clientNomEdit = nullptr;
    QLineEdit *m_clientPrenomEdit = nullptr;
    QLineEdit *m_clientEmailEdit = nullptr;
    QCheckBox *m_chkClientBloque = nullptr;
    QLineEdit *m_clientNouveauMdp = nullptr;

    QLineEdit *m_smtpServeur = nullptr;
    QSpinBox *m_smtpPort = nullptr;
    QLineEdit *m_smtpIdentifiant = nullptr;
    QLineEdit *m_smtpEmail = nullptr;
    QLineEdit *m_smtpMdpApp = nullptr;
    QLineEdit *m_smtpNom = nullptr;
    QLineEdit *m_paiementMobileMoneyCfg = nullptr;
    QLineEdit *m_paiementCarteBancaireCfg = nullptr;

    QTableView *m_tableAdmins = nullptr;
    QSqlQueryModel *m_modeleAdmins = nullptr;
    QLineEdit *m_adminNom = nullptr;
    QLineEdit *m_adminPrenom = nullptr;
    QLineEdit *m_adminEmail = nullptr;
    QLineEdit *m_adminMdp = nullptr;
};

#endif // MAINWINDOW_H
