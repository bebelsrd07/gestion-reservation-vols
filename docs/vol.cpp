#include "vol.h"

Vol::Vol(int id, const QString &numero, const QString &compagnie,
         const QString &depart, const QString &arrivee,
         const QDateTime &dateDepart, double prix, int places)
    : m_idVol(id)
    , m_numeroVol(numero)
    , m_compagnie(compagnie)
    , m_villeDepart(depart)
    , m_villeArrivee(arrivee)
    , m_dateDepart(dateDepart)
    , m_prix(prix)
    , m_placesDisponibles(places)
{
}

int Vol::getIdVol() const { return m_idVol; }
QString Vol::getNumeroVol() const { return m_numeroVol; }
QString Vol::getCompagnie() const { return m_compagnie; }
QString Vol::getVilleDepart() const { return m_villeDepart; }
QString Vol::getVilleArrivee() const { return m_villeArrivee; }
QDateTime Vol::getDateDepart() const { return m_dateDepart; }
double Vol::getPrix() const { return m_prix; }
int Vol::getPlacesDisponibles() const { return m_placesDisponibles; }

bool Vol::estDisponible(int nbPlacesDemandees) const
{
    return m_placesDisponibles >= nbPlacesDemandees;
}