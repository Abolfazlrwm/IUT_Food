#include "mainwindow.h"
#include "adminpanel.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AdminPanel w;
    w.show();
    return a.exec();
}
