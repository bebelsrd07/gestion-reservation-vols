#ifndef BILLETPDF_H
#define BILLETPDF_H

#include "modeles.h"
#include <QString>

// Génération d'un billet d'avion électronique unique au format PDF regroupant tous les passagers.
namespace BilletPdf {

    bool genererBilletUnique(const DetailsReservation &reservation,
                             const QString &cheminFichierPdf,
                             QString &erreur);

    QString cheminParDefautBillet(const QString &reference);

} // namespace BilletPdf

#endif // BILLETPDF_H
