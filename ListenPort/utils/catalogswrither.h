#ifndef CATALOGSWRITHER_H
#define CATALOGSWRITHER_H
#include <QByteArray>

class catalogswrither
{
public:
    catalogswrither();
    void writeCatalogToFile(QByteArray data);
    void getCatalogByFile();
    QByteArray prepearFile();
};

#endif // CATALOGSWRITHER_H
