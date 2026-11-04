#include "smtpclient.h"
#include "constantes.h"
#include "databasemanager.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QSslSocket>

namespace SmtpClient {

ConfigSmtp chargerConfiguration()
{
    QSettings s(DatabaseManager::cheminFichierConfig(), QSettings::IniFormat);
    ConfigSmtp cfg;
    cfg.serveur = s.value(QStringLiteral("smtp/serveur"),
                          QStringLiteral("smtp.gmail.com")).toString().trimmed();
    cfg.port = s.value(QStringLiteral("smtp/port"),
                       Constantes::PORT_SMTP_SSL_DEFAUT).toInt();
    cfg.identifiantSmtp = s.value(QStringLiteral("smtp/identifiant"),
                                  QString()).toString().trimmed();
    cfg.emailExpediteur = s.value(QStringLiteral("smtp/email_expediteur"),
                                  QString()).toString().trimmed();
    cfg.motDePasseApplication = s.value(QStringLiteral("smtp/mot_de_passe_application"),
                                        QString()).toString().remove(QLatin1Char(' '));
    cfg.nomExpediteur = s.value(QStringLiteral("smtp/nom_expediteur"),
                                QStringLiteral("Agence Billets d'Avion")).toString().trimmed();
    return cfg;
}

void sauvegarderConfiguration(const ConfigSmtp &cfg)
{
    QSettings s(DatabaseManager::cheminFichierConfig(), QSettings::IniFormat);
    s.setValue(QStringLiteral("smtp/serveur"), cfg.serveur.trimmed());
    s.setValue(QStringLiteral("smtp/port"), cfg.port);
    s.setValue(QStringLiteral("smtp/identifiant"), cfg.identifiantSmtp.trimmed());
    s.setValue(QStringLiteral("smtp/email_expediteur"), cfg.emailExpediteur.trimmed());
    s.setValue(QStringLiteral("smtp/mot_de_passe_application"),
               cfg.motDePasseApplication.trimmed().remove(QLatin1Char(' ')));
    s.setValue(QStringLiteral("smtp/nom_expediteur"), cfg.nomExpediteur.trimmed());
    s.sync();
}

static QByteArray decouperBase64Lignes76(const QByteArray &donneesBrutes)
{
    QByteArray b64 = donneesBrutes.toBase64();
    QByteArray resultat;
    constexpr int LARGEUR_LIGNE_MIME = 76;
    resultat.reserve(b64.size() + (b64.size() / LARGEUR_LIGNE_MIME) * 2 + 4);

    for (int i = 0; i < b64.size(); i += LARGEUR_LIGNE_MIME) {
        resultat.append(b64.mid(i, LARGEUR_LIGNE_MIME));
        resultat.append("\r\n");
    }
    return resultat;
}

static bool lireReponseComplete(QSslSocket &socket, int &codeStatut, QString &texteComplet)
{
    texteComplet.clear();
    codeStatut = 0;

    while (true) {
        while (!socket.canReadLine()) {
            if (!socket.waitForReadyRead(Constantes::DELAI_TIMEOUT_SMTP_MS)) {
                texteComplet = QStringLiteral("Délai d'attente SMTP dépassé : ")
                               + socket.errorString();
                return false;
            }
        }

        while (socket.canReadLine()) {
            QString ligne = QString::fromUtf8(socket.readLine()).trimmed();
            texteComplet += ligne + QLatin1Char('\n');
            if (ligne.size() >= 3) {
                bool ok = false;
                int code = ligne.left(3).toInt(&ok);
                if (ok && (ligne.size() == 3 || ligne.at(3) == QLatin1Char(' '))) {
                    codeStatut = code;
                    return true;
                }
            }
        }
    }
}

static bool envoyerCommandeEtVerifier(QSslSocket &socket,
                                      const QByteArray &commande,
                                      int codeAttendu,
                                      QString &erreur)
{
    socket.write(commande);
    if (!socket.waitForBytesWritten(Constantes::DELAI_TIMEOUT_SMTP_MS)) {
        erreur = QStringLiteral("Erreur d'écriture réseau SMTP : ") + socket.errorString();
        return false;
    }

    int codeRecu = 0;
    QString detail;
    if (!lireReponseComplete(socket, codeRecu, detail)) {
        erreur = detail;
        return false;
    }

    if (codeRecu != codeAttendu) {
        erreur = QStringLiteral("Réponse SMTP inattendue (%1) : %2")
                     .arg(codeRecu)
                     .arg(detail.trimmed());
        return false;
    }
    return true;
}

static bool transmettreMimeSmtp(const ConfigSmtp &cfg,
                                const QString &emailDestinataire,
                                const QByteArray &messageMimeComplet,
                                QString &erreur)
{
    if (!cfg.estConfigure()) {
        erreur = QStringLiteral("SMTP non configuré (renseignez l'onglet Email SMTP ou config.ini).");
        return false;
    }

    const QString loginSmtp = cfg.identifiantSmtp.trimmed().isEmpty()
                                  ? cfg.emailExpediteur.trimmed()
                                  : cfg.identifiantSmtp.trimmed();
    QString adresseFrom = cfg.emailExpediteur.trimmed();
    if (!adresseFrom.contains(QLatin1Char('@')))
        adresseFrom = QStringLiteral("noreply@billets-avion.mg");

    QSslSocket socket;
    int codeInitial = 0;
    QString banniere;

    if (cfg.port == Constantes::PORT_SMTP_SSL_DEFAUT) {
        socket.connectToHostEncrypted(cfg.serveur, static_cast<quint16>(cfg.port));
        if (!socket.waitForEncrypted(Constantes::DELAI_TIMEOUT_SMTP_MS)) {
            erreur = QStringLiteral("Échec de connexion SSL au serveur SMTP : ")
                     + socket.errorString();
            return false;
        }
        if (!lireReponseComplete(socket, codeInitial, banniere) || codeInitial != 220) {
            erreur = QStringLiteral("Serveur SMTP indisponible : ") + banniere.trimmed();
            return false;
        }
    } else {
        socket.connectToHost(cfg.serveur, static_cast<quint16>(cfg.port));
        if (!socket.waitForConnected(Constantes::DELAI_TIMEOUT_SMTP_MS)) {
            erreur = QStringLiteral("Échec de connexion TCP au serveur SMTP : ")
                     + socket.errorString();
            return false;
        }
        if (!lireReponseComplete(socket, codeInitial, banniere) || codeInitial != 220) {
            erreur = QStringLiteral("Serveur SMTP indisponible : ") + banniere.trimmed();
            return false;
        }
        if (!envoyerCommandeEtVerifier(socket, "EHLO localhost\r\n", 250, erreur))
            return false;
        if (!envoyerCommandeEtVerifier(socket, "STARTTLS\r\n", 220, erreur))
            return false;
        socket.startClientEncryption();
        if (!socket.waitForEncrypted(Constantes::DELAI_TIMEOUT_SMTP_MS)) {
            erreur = QStringLiteral("Échec de négociation STARTTLS : ") + socket.errorString();
            return false;
        }
    }

    if (!envoyerCommandeEtVerifier(socket, "EHLO localhost\r\n", 250, erreur))
        return false;
    if (!envoyerCommandeEtVerifier(socket, "AUTH LOGIN\r\n", 334, erreur))
        return false;
    if (!envoyerCommandeEtVerifier(
            socket, loginSmtp.toUtf8().toBase64() + "\r\n", 334, erreur)) {
        return false;
    }
    if (!envoyerCommandeEtVerifier(
            socket, cfg.motDePasseApplication.toUtf8().toBase64() + "\r\n", 235, erreur)) {
        erreur = QStringLiteral("Authentification SMTP refusée : ") + erreur;
        return false;
    }

    if (!envoyerCommandeEtVerifier(
            socket, "MAIL FROM:<" + adresseFrom.toUtf8() + ">\r\n", 250, erreur)) {
        return false;
    }
    if (!envoyerCommandeEtVerifier(
            socket, "RCPT TO:<" + emailDestinataire.trimmed().toUtf8() + ">\r\n", 250, erreur)) {
        return false;
    }
    if (!envoyerCommandeEtVerifier(socket, "DATA\r\n", 354, erreur))
        return false;
    if (!envoyerCommandeEtVerifier(socket, messageMimeComplet, 250, erreur))
        return false;

    QString ignoreQuit;
    envoyerCommandeEtVerifier(socket, "QUIT\r\n", 221, ignoreQuit);
    socket.disconnectFromHost();
    erreur.clear();
    return true;
}

bool envoyerEmailConfirmation(const ConfigSmtp &cfg,
                              const DetailsReservation &reservation,
                              const QString &cheminPieceJointePdf,
                              QString &erreur,
                              bool estModificationVol)
{
    QFile fichierPdf(cheminPieceJointePdf);
    if (!fichierPdf.open(QIODevice::ReadOnly)) {
        erreur = QStringLiteral("Impossible de lire le billet PDF à joindre à l'email.");
        return false;
    }
    QByteArray octetsPdf = fichierPdf.readAll();
    fichierPdf.close();

    QString titreBandeau = estModificationVol
                               ? QStringLiteral("VOL MODIFIÉ — NOUVEAU BILLET PDF")
                               : QStringLiteral("CONFIRMATION DE VOTRE BILLET");

    QString introHtml = estModificationVol
                            ? QStringLiteral("Un vol de votre réservation <b>%1</b> a été modifié par la compagnie.<br>"
                                             "Veuillez trouver en pièce jointe votre <b>nouveau billet électronique PDF mis à jour</b>.")
                                  .arg(reservation.reference.toHtmlEscaped())
                            : QStringLiteral("Votre réservation <b>%1</b> a été confirmée par <b>TicketFly</b>.<br>"
                                             "Veuillez trouver en pièce jointe votre billet électronique PDF regroupant tous les passagers.")
                                  .arg(reservation.reference.toHtmlEscaped());

    QString volsHtml;
    for (int i = 0; i < reservation.troncons.size(); ++i) {
        const TronconVol &t = reservation.troncons[i];
        volsHtml += QStringLiteral(
                        "<div style='background:#eef6f3;border:2px solid #001e1d;border-radius:8px;padding:10px 14px;margin-bottom:8px;'>"
                        "<div style='font-weight:bold;color:#004643;'>Tronçon %1 — Vol %2 (%3)</div>"
                        "<div style='margin-top:4px;color:#001e1d;'>%4 (%5) &rarr; %6 (%7) &nbsp;|&nbsp; Départ : <b>%8</b></div>"
                        "</div>")
                        .arg(i + 1)
                        .arg(t.numeroVol.toHtmlEscaped(),
                             t.compagnie.toHtmlEscaped(),
                             t.depart.ville.toHtmlEscaped(),
                             t.depart.codeIata.toHtmlEscaped(),
                             t.arrivee.ville.toHtmlEscaped(),
                             t.arrivee.codeIata.toHtmlEscaped(),
                             t.dateDepart.toString(QStringLiteral("dd/MM/yyyy HH:mm")));
    }

    QString passagersHtml;
    for (const PassagerInfo &p : reservation.passagers) {
        passagersHtml += QStringLiteral(
                             "<tr>"
                             "<td style='padding:8px 10px;border-bottom:1px solid #abd1c6;'>%1 %2</td>"
                             "<td style='padding:8px 10px;border-bottom:1px solid #abd1c6;'>%3</td>"
                             "<td style='padding:8px 10px;border-bottom:1px solid #abd1c6;font-weight:bold;color:#004643;'>%4</td>"
                             "</tr>")
                             .arg(p.nom.toHtmlEscaped(),
                                  p.prenom.toHtmlEscaped(),
                                  p.numeroPasseport.toHtmlEscaped(),
                                  p.siege.toHtmlEscaped());
    }

    QString corpsHtml = QStringLiteral(
                            "<!DOCTYPE html><html><body style='margin:0;padding:24px;background:#abd1c6;font-family:Segoe UI,Arial,sans-serif;color:#001e1d;'>"
                            "<div style='max-width:620px;margin:0 auto;background:#fffffe;border:3px solid #001e1d;border-radius:12px;overflow:hidden;'>"
                            "  <div style='background:#004643;color:#fffffe;padding:20px 24px;border-bottom:3px solid #001e1d;'>"
                            "    <span style='display:inline-block;background:#f9bc60;color:#001e1d;font-weight:bold;padding:4px 10px;border:2px solid #001e1d;border-radius:6px;font-size:12px;'>&#9992; TICKETFLY</span>"
                            "    <h2 style='margin:10px 0 0 0;font-size:20px;color:#fffffe;'>%1</h2>"
                            "  </div>"
                            "  <div style='padding:22px 24px;'>"
                            "    <p style='margin-top:0;'>Bonjour <b>%2 %3</b>,</p>"
                            "    <p>%4</p>"
                            "    <h3 style='color:#004643;margin-bottom:8px;'>Récapitulatif des vols</h3>"
                            "    %5"
                            "    <h3 style='color:#004643;margin-bottom:8px;'>Passagers</h3>"
                            "    <table style='width:100%;border-collapse:collapse;border:2px solid #001e1d;font-size:13px;'>"
                            "      <thead><tr style='background:#004643;color:#fffffe;text-align:left;'>"
                            "        <th style='padding:8px 10px;'>Identité</th>"
                            "        <th style='padding:8px 10px;'>Passeport</th>"
                            "        <th style='padding:8px 10px;'>Siège</th>"
                            "      </tr></thead>"
                            "      <tbody>%6</tbody>"
                            "    </table>"
                            "    <div style='margin-top:16px;background:#f9bc60;color:#001e1d;border:2px solid #001e1d;border-radius:8px;padding:10px 14px;font-weight:bold;'>"
                            "      Montant total : %7 EUR"
                            "    </div>"
                            "    <p style='margin-top:16px;margin-bottom:0;font-size:12px;color:#004643;'>Bon voyage avec %8 !</p>"
                            "  </div>"
                            "</div></body></html>")
                            .arg(titreBandeau,
                                 reservation.prenomClient.toHtmlEscaped(),
                                 reservation.nomClient.toHtmlEscaped(),
                                 introHtml,
                                 volsHtml,
                                 passagersHtml,
                                 QString::number(reservation.prixTotal, 'f', 2),
                                 cfg.nomExpediteur.toHtmlEscaped());

    QString adresseFrom = cfg.emailExpediteur.trimmed();
    if (!adresseFrom.contains(QLatin1Char('@')))
        adresseFrom = QStringLiteral("noreply@billets-avion.mg");

    QString sujetBrut = (estModificationVol
                             ? QStringLiteral("TicketFly — Vol modifié et nouveau billet - ")
                             : QStringLiteral("TicketFly — Confirmation de votre billet - "))
                        + reservation.reference;
    QByteArray sujetEncode = "=?UTF-8?B?" + sujetBrut.toUtf8().toBase64() + "?=";
    QByteArray expediteurEncode = "=?UTF-8?B?" + cfg.nomExpediteur.toUtf8().toBase64() + "?=";
    QByteArray frontiere = "====FRONTIERE_BILLET_AVION_2026====";
    QString nomPdf = QFileInfo(cheminPieceJointePdf).fileName();

    QByteArray mime;
    mime += "From: " + expediteurEncode + " <" + adresseFrom.toUtf8() + ">\r\n";
    mime += "To: <" + reservation.emailClient.toUtf8() + ">\r\n";
    mime += "Subject: " + sujetEncode + "\r\n";
    mime += "Date: "
            + QLocale::c().toString(QDateTime::currentDateTimeUtc(),
                                    QStringLiteral("ddd, dd MMM yyyy HH:mm:ss")).toUtf8()
            + " +0000\r\n";
    mime += "MIME-Version: 1.0\r\n";
    mime += "Content-Type: multipart/mixed; boundary=\"" + frontiere + "\"\r\n\r\n";

    mime += "--" + frontiere + "\r\n";
    mime += "Content-Type: text/html; charset=\"UTF-8\"\r\n";
    mime += "Content-Transfer-Encoding: base64\r\n\r\n";
    mime += decouperBase64Lignes76(corpsHtml.toUtf8()) + "\r\n";

    mime += "--" + frontiere + "\r\n";
    mime += "Content-Type: application/pdf; name=\"" + nomPdf.toUtf8() + "\"\r\n";
    mime += "Content-Transfer-Encoding: base64\r\n";
    mime += "Content-Disposition: attachment; filename=\"" + nomPdf.toUtf8() + "\"\r\n\r\n";
    mime += decouperBase64Lignes76(octetsPdf) + "\r\n";
    mime += "--" + frontiere + "--\r\n.\r\n";

    return transmettreMimeSmtp(cfg, reservation.emailClient, mime, erreur);
}

bool envoyerEmailTexteSimple(const ConfigSmtp &cfg,
                             const QString &emailDestinataire,
                             const QString &sujet,
                             const QString &corpsTexte,
                             QString &erreur)
{
    QString adresseFrom = cfg.emailExpediteur.trimmed();
    if (!adresseFrom.contains(QLatin1Char('@')))
        adresseFrom = QStringLiteral("noreply@billets-avion.mg");

    QString corpsHtml = QStringLiteral(
                            "<!DOCTYPE html><html><body style='margin:0;padding:24px;background:#abd1c6;font-family:Segoe UI,Arial,sans-serif;color:#001e1d;'>"
                            "<div style='max-width:580px;margin:0 auto;background:#fffffe;border:3px solid #001e1d;border-radius:12px;overflow:hidden;'>"
                            "  <div style='background:#004643;color:#fffffe;padding:16px 22px;border-bottom:3px solid #001e1d;'>"
                            "    <span style='display:inline-block;background:#f9bc60;color:#001e1d;font-weight:bold;padding:3px 10px;border:2px solid #001e1d;border-radius:6px;font-size:12px;'>&#9992; TICKETFLY</span>"
                            "    <h3 style='margin:8px 0 0 0;color:#fffffe;'>%1</h3>"
                            "  </div>"
                            "  <div style='padding:20px 22px;line-height:1.5;'>%2</div>"
                            "</div></body></html>")
                            .arg(sujet.toHtmlEscaped(),
                                 corpsTexte.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));

    QByteArray sujetEncode = "=?UTF-8?B?" + sujet.toUtf8().toBase64() + "?=";
    QByteArray expediteurEncode = "=?UTF-8?B?" + cfg.nomExpediteur.toUtf8().toBase64() + "?=";

    QByteArray mime;
    mime += "From: " + expediteurEncode + " <" + adresseFrom.toUtf8() + ">\r\n";
    mime += "To: <" + emailDestinataire.trimmed().toUtf8() + ">\r\n";
    mime += "Subject: " + sujetEncode + "\r\n";
    mime += "Date: "
            + QLocale::c().toString(QDateTime::currentDateTimeUtc(),
                                    QStringLiteral("ddd, dd MMM yyyy HH:mm:ss")).toUtf8()
            + " +0000\r\n";
    mime += "MIME-Version: 1.0\r\n";
    mime += "Content-Type: text/html; charset=\"UTF-8\"\r\n";
    mime += "Content-Transfer-Encoding: base64\r\n\r\n";
    mime += decouperBase64Lignes76(corpsHtml.toUtf8()) + "\r\n.\r\n";

    return transmettreMimeSmtp(cfg, emailDestinataire, mime, erreur);
}

} // namespace SmtpClient