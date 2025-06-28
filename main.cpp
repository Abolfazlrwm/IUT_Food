#include "client.h"
#include <QApplication>
#include <QStyleFactory> // <<< این هدر را اضافه کنید

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // این خط، استایل برنامه را به "Fusion" که یک استایل استاندارد و روشن است، تغییر می‌دهد
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    Client w;
    w.show();
    return a.exec();
}
