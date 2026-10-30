#ifndef ESCALEDIALOG_H
#define ESCALEDIALOG_H

#include <QDialog>

class DatabaseManager;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QTableWidget;

class EscaleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EscaleDialog(DatabaseManager *db, int idUtilisateur,
                          QWidget *parent = nullptr);

private slots:
    void rechercher();
    void reserver(int row);

private:
    DatabaseManager *m_db;
    int m_idUtilisateur;
    QLineEdit *m_depart;
    QLineEdit *m_arrivee;
    QComboBox *m_nat;
    QSpinBox *m_nb;
    QCheckBox *m_risque;
    QLabel *m_info;
    QTableWidget *m_table;
};

#endif // ESCALEDIALOG_H