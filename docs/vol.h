#pragma once

#include <QString>
#include <QDateTime>

class Vol
{
public:
    Vol(int id, const QString &numero, const QString &compagnie,
        const QString &depart, const QString &arrivee,
        const QDateTime &dateDepart, double prix, int places);

    int getIdVol() const;
    QString getNumeroVol() const;
    QString getCompagnie() const;
    QString getVilleDepart() const;
    QString getVilleArrivee() const;
    QDateTime getDateDepart() const;
    double getPrix() const;
    int getPlacesDisponibles() const;

    // Règle métier : vérifier la disponibilité
    bool estDisponible(int nbPlacesDemandees = 1) const;

private:
    int m_idVol;
    QString m_numeroVol;
    QString m_compagnie;
    QString m_villeDepart;
    QString m_villeArrivee;
    QDateTime m_dateDepart;
    double m_prix;
    int m_placesDisponibles;
};