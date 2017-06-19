#ifndef QTREEJSONWIDGET_H
#define QTREEJSONWIDGET_H

#include <QObject>
#include <string>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QTreeWidgetItem>
#include <QTreeWidget>
#include "changesettingswindow.h"


class QTreeJsonWidget : public QObject
{
    Q_OBJECT
public:
    explicit QTreeJsonWidget(QObject *parent = 0);
    QTreeWidget generateTree(std::string json);

signals:

public slots:
    void changeRow();
    void addRow();
    void delRow();
    void addChildRow();

private:
    ChangeSettingsWindow * change_sett_wind;
    bool loadJson(const QByteArray &json);
    QTreeWidgetItem * load(const QJsonValue& value, QTreeWidgetItem* parent);

    QTreeWidgetItem * mParent;
    QTreeWidgetItem * mRootItem;
    QJsonDocument mDocument;
    QTreeWidget *tree;
};

#endif // QTREEJSONWIDGET_H
