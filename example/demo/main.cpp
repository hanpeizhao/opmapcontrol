/**
******************************************************************************
*
* @file       main.cpp
* @brief      示例程序入口
* @see        The GNU Public License (GPL) Version 3
* @{
*
*****************************************************************************/

#include <QtWidgets/QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow win;
    win.showMaximized();

    return app.exec();
}
