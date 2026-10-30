#ifndef SEATMAPDIALOG_H
#define SEATMAPDIALOG_H

#include <QDialog>
#include <QMap>
#include <QList>

class DatabaseManager;
class QPushButton;
class QLabel;

class SeatMapDialog : public QDialog
{
    Q_OBJECT
public:
    SeatMapDialog(DatabaseManager *db, int idVol, int nb, const QString &titre,
                  QWidget *parent = nullptr);

    QList<int> sieges() const;              // id des sièges choisis

private slots:
    void majInfos();
    void placementAuto();
    void effacer();

private:
    int nbChoisis() const;

    DatabaseManager *m_db;
    int m_idVol;
    int m_nb;
    QMap<int, QPushButton *> m_boutons;     // id_siege -> bouton
    QMap<int, double> m_supp;               // id_siege -> supplément
    QMap<int, QString> m_num;               // id_siege -> numéro (12A)
    QLabel *m_info;
    QPushButton *m_valider;
};

#endif // SEATMAPDIALOG_H