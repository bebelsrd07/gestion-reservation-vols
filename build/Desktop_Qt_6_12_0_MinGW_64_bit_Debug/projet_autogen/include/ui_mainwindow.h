/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDateEdit>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QLineEdit *editDepart;
    QLineEdit *editArrivee;
    QCheckBox *chkDate;
    QDateEdit *dateDepart;
    QSpinBox *spinPassagers;
    QPushButton *btnRechercher;
    QTableView *tableVols;
    QPushButton *btnReservations;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 325);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        editDepart = new QLineEdit(centralwidget);
        editDepart->setObjectName("editDepart");

        verticalLayout->addWidget(editDepart);

        editArrivee = new QLineEdit(centralwidget);
        editArrivee->setObjectName("editArrivee");

        verticalLayout->addWidget(editArrivee);

        chkDate = new QCheckBox(centralwidget);
        chkDate->setObjectName("chkDate");

        verticalLayout->addWidget(chkDate);

        dateDepart = new QDateEdit(centralwidget);
        dateDepart->setObjectName("dateDepart");
        dateDepart->setCalendarPopup(true);

        verticalLayout->addWidget(dateDepart);

        spinPassagers = new QSpinBox(centralwidget);
        spinPassagers->setObjectName("spinPassagers");
        spinPassagers->setMinimum(1);

        verticalLayout->addWidget(spinPassagers);

        btnRechercher = new QPushButton(centralwidget);
        btnRechercher->setObjectName("btnRechercher");

        verticalLayout->addWidget(btnRechercher);

        tableVols = new QTableView(centralwidget);
        tableVols->setObjectName("tableVols");

        verticalLayout->addWidget(tableVols);

        btnReservations = new QPushButton(centralwidget);
        btnReservations->setObjectName("btnReservations");

        verticalLayout->addWidget(btnReservations);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        editDepart->setPlaceholderText(QCoreApplication::translate("MainWindow", "Ville de d\303\251part", nullptr));
        editArrivee->setPlaceholderText(QCoreApplication::translate("MainWindow", "Ville d'arriv\303\251e", nullptr));
        chkDate->setText(QCoreApplication::translate("MainWindow", "Filtrer par date de d\303\251part", nullptr));
        spinPassagers->setPrefix(QCoreApplication::translate("MainWindow", "Passagers : ", nullptr));
        btnRechercher->setText(QCoreApplication::translate("MainWindow", "Rechercher", nullptr));
        btnReservations->setText(QCoreApplication::translate("MainWindow", "Mes r\303\251servations", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
