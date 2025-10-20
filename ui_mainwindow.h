/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCharts/QChartView>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QPushButton *loginButton;
    QHBoxLayout *statusLayout;
    QLabel *statusLabel;
    QLabel *fundsLabel;
    QHBoxLayout *horizontalLayout;
    QComboBox *instrumentComboBox;
    QComboBox *intervalComboBox;
    QChartView *chartView;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        loginButton = new QPushButton(centralwidget);
        loginButton->setObjectName("loginButton");
        loginButton->setStyleSheet(QString::fromUtf8("font: 700 11pt \"Segoe UI\";\n"
"color: rgb(0, 0, 0);\n"
"background-color: rgb(170, 170, 127);"));

        verticalLayout->addWidget(loginButton);

        statusLayout = new QHBoxLayout();
        statusLayout->setObjectName("statusLayout");
        statusLabel = new QLabel(centralwidget);
        statusLabel->setObjectName("statusLabel");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(statusLabel->sizePolicy().hasHeightForWidth());
        statusLabel->setSizePolicy(sizePolicy);
        statusLabel->setStyleSheet(QString::fromUtf8("font: 700 9pt \"Segoe UI\";\n"
"color: rgb(112, 21, 127);"));

        statusLayout->addWidget(statusLabel);

        fundsLabel = new QLabel(centralwidget);
        fundsLabel->setObjectName("fundsLabel");
        sizePolicy.setHeightForWidth(fundsLabel->sizePolicy().hasHeightForWidth());
        fundsLabel->setSizePolicy(sizePolicy);
        fundsLabel->setStyleSheet(QString::fromUtf8("font: 700 9pt \"Segoe UI\";\n"
"color: rgb(21, 112,21);"));
        fundsLabel->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        statusLayout->addWidget(fundsLabel);


        verticalLayout->addLayout(statusLayout);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        instrumentComboBox = new QComboBox(centralwidget);
        instrumentComboBox->setObjectName("instrumentComboBox");

        horizontalLayout->addWidget(instrumentComboBox);

        intervalComboBox = new QComboBox(centralwidget);
        intervalComboBox->setObjectName("intervalComboBox");

        horizontalLayout->addWidget(intervalComboBox);


        verticalLayout->addLayout(horizontalLayout);

        chartView = new QChartView(centralwidget);
        chartView->setObjectName("chartView");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(3);
        sizePolicy1.setHeightForWidth(chartView->sizePolicy().hasHeightForWidth());
        chartView->setSizePolicy(sizePolicy1);

        verticalLayout->addWidget(chartView);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 25));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "QphoeniX Trading", nullptr));
        loginButton->setText(QCoreApplication::translate("MainWindow", "Login", nullptr));
        statusLabel->setText(QCoreApplication::translate("MainWindow", "User: N/A", nullptr));
        fundsLabel->setText(QCoreApplication::translate("MainWindow", "Funds: N/A", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
