/********************************************************************************
** Form generated from reading UI file 'QSPlayer.ui'
**
** Created by: Qt User Interface Compiler version 5.9.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_QSPLAYER_H
#define UI_QSPLAYER_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>


QT_BEGIN_NAMESPACE

class Ui_QSPlayerClass
{
public:
    

    void setupUi(QMainWindow *QSPlayerClass)
    {
        if (QSPlayerClass->objectName().isEmpty())
            QSPlayerClass->setObjectName(QStringLiteral("QSPlayerClass"));
        QSPlayerClass->resize(600, 400);
        

        retranslateUi(QSPlayerClass);

        QMetaObject::connectSlotsByName(QSPlayerClass);
    } // setupUi

    void retranslateUi(QMainWindow *QSPlayerClass)
    {
        QSPlayerClass->setWindowTitle(QApplication::translate("QSPlayerClass", "QSPlayer", Q_NULLPTR));
    } // retranslateUi

};

namespace Ui {
    class QSPlayerClass: public Ui_QSPlayerClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_QSPLAYER_H
