#ifndef VOYAGEDIALOG_H
#define VOYAGEDIALOG_H

#include <QDialog>

class DatabaseManager;
class QTableWidget;

class VoyageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VoyageDialog(DatabaseManager *db, int idUtilisateur,
                          QWidget *parent = nullptr);

private slots:
    void charger();
    void toutLire();
    void alternatives();
    void droits();

private:
    DatabaseManager *m_db;
    int m_id;
    QTableWidget *m_res;
    QTableWidget *m_alertes;
};

#endif // VOYAGEDIALOG_H