#ifndef VOL_H
#define VOL_H

#include <QString>

class Vol
{
public:
    Vol() = default;
    Vol(int id, const QString &numero, const QString &depart,
        const QString &arrivee, double prix, int places)
        : m_id(id), m_numero(numero), m_depart(depart),
        m_arrivee(arrivee), m_prix(prix), m_places(places) {}

    int id() const { return m_id; }
    QString numeroVol() const { return m_numero; }
    QString villeDepart() const { return m_depart; }
    QString villeArrivee() const { return m_arrivee; }
    double prix() const { return m_prix; }
    int placesDisponibles() const { return m_places; }
    bool estComplet() const { return m_places <= 0; }

private:
    int m_id = 0;
    QString m_numero, m_depart, m_arrivee;
    double m_prix = 0.0;
    int m_places = 0;
};

#endif // VOL_H