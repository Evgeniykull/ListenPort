#ifndef JSONCONVERTOR_H
#define JSONCONVERTOR_H
#include <QByteArray>
#include <QTreeWidget>

class JsonConvertor
{
public:
    JsonConvertor();
    JsonConvertor(int);

    QString dataToJson(QByteArray data);
    QString jsonToData(QByteArray data);
    QString dataByTree(QTreeWidget *tree_struc);

private:
    void tree(QTreeWidgetItem * item, QString & data, bool is_mass = false);
    int precision = 2;
};

#endif // JSONCONVERTOR_H
