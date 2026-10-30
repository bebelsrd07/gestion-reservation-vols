#include "reservation.h"
#include <QUuid>

Reservation::Reservation(int idVol, const QString &passager,
                         int nbPlaces, double prixUnitaire)
    : m_idVol(idVol), m_passager(passager),
    m_nbPlaces(nbPlaces), m_prixUnitaire(prixUnitaire),
    m_reference(genererReference()) {}

QString Reservation::genererReference()
{
    return "RES-" + QUuid::createUuid().toString(QUuid::WithoutBraces)
    .left(8).toUpper();
}