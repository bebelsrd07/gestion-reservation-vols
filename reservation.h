#ifndef RESERVATION_H
#define RESERVATION_H

#include <QString>
#include <QDateTime>

class Reservation
{
public:
    Reservation(int idVol, const QString &passager, int nbPlaces, double prixUnitaire);

    QString reference() const { return m_reference; }
    double prixTotal() const { return m_nbPlaces * m_prixUnitaire; }
    int idVol() const { return m_idVol; }
    QString passager() const { return m_passager; }
    int nbPlaces() const { return m_nbPlaces; }

private:
    static QString genererReference();

    int m_idVol;
    QString m_passager;
    int m_nbPlaces;
    double m_prixUnitaire;
    QString m_reference;
};

#endif // RESERVATION_H