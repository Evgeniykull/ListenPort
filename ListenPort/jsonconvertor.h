#ifndef JSONCONVERTOR_H
#define JSONCONVERTOR_H
#include <QByteArray>
#include <QTreeWidget>

class JsonConvertor
{
public:
    JsonConvertor();

    QString dataToJson(QByteArray data);
    QString jsonToData(QByteArray data);
    QString dataByTree(QTreeWidget *tree_struc);
};

#endif // JSONCONVERTOR_H
