#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

class DatabaseManager;
class QLineEdit;
class QTabWidget;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(DatabaseManager *db, QWidget *parent = nullptr);

    int idUtilisateur() const { return m_id; }
    QString role() const { return m_role; }
    QString nomComplet() const { return m_nom; }

private slots:
    void seConnecter();
    void sInscrire();

private:
    DatabaseManager *m_db;
    int m_id = 0;
    QString m_role;
    QString m_nom;

    QTabWidget *m_onglets;
    QLineEdit *m_loginEmail;
    QLineEdit *m_loginMdp;
    QLineEdit *m_nomEdit;
    QLineEdit *m_prenomEdit;
    QLineEdit *m_emailEdit;
    QLineEdit *m_mdpEdit;
    QLineEdit *m_mdp2Edit;
};

#endif // LOGINDIALOG_H