#ifndef CATALOGSWRITHER_H
#define CATALOGSWRITHER_H
#include <QByteArray>
#include <QTime>
#include <QCoreApplication>


class catalogswrither
{
public:
    catalogswrither();
    void writeCatalogToFile(QByteArray data);
    void getCatalogByFile();
    QByteArray prepearFile();
};

int Calc_hCRC(unsigned char * bf, uint len);
void delay(int num);

#endif // CATALOGSWRITHER_H
