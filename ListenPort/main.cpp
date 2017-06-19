#include "listenport.h"
#include <QApplication>
#include <QTreeView>
#include <QFile>
#include <string>
#include "qjsonmodel.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ListenPort w;
    w.show();

    return a.exec();
}
