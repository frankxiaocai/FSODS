#include "mainwindow.h"
#include <QApplication>
#include <windows.h>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFile qssFile(a.applicationDirPath() + "/qss/industrial.qss");
    if(qssFile.open(QIODevice::ReadOnly))
    {
        QString style = qssFile.readAll();
        a.setStyleSheet(style);
        qssFile.close();
    }

    MainWindow w;
    w.setWindowFlags(Qt::FramelessWindowHint);
    w.show();
    return QCoreApplication::exec();
}
