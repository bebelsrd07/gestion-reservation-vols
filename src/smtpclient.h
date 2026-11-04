#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include "modeles.h"
#include <QString>

struct ConfigSmtp {
    QString serveur = QStringLiteral("smtp.gmail.com");
    int port = 465;
    QString identifiantSmtp; // Optionnel (ex: Username Mailtrap) ; sinon utilise emailExpediteur
    QString emailExpediteur;
    QString motDePasseApplication;
    QString nomExpediteur = QStringLiteral("TicketFly");

    bool estConfigure() const
    {
        const QString login = identifiantSmtp.trimmed().isEmpty()
                                  ? emailExpediteur.trimmed()
                                  : identifiantSmtp.trimmed();
        return !serveur.trimmed().isEmpty()
               && !login.isEmpty()
               && !motDePasseApplication.trimmed().isEmpty();
    }
};

namespace SmtpClient {

    ConfigSmtp chargerConfiguration();
    void sauvegarderConfiguration(const ConfigSmtp &cfg);

    // Envoi réel par SMTP sécurisé (confirmation initiale ou nouveau billet après modification de vol)
    bool envoyerEmailConfirmation(const ConfigSmtp &cfg,
                                  const DetailsReservation &reservation,
                                  const QString &cheminPieceJointePdf,
                                  QString &erreur,
                                  bool estModificationVol = false);

    // Envoi d'un email texte simple (module d'assistance / support)
    bool envoyerEmailTexteSimple(const ConfigSmtp &cfg,
                                 const QString &emailDestinataire,
                                 const QString &sujet,
                                 const QString &corpsTexte,
                                 QString &erreur);

} // namespace SmtpClient

#endif // SMTPCLIENT_H
