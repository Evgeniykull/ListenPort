#include "qtreejsonwidget.h"
#include <QWidget>
#include <QDebug>
#include <QIcon>
#include <QDialog>
#include <QDebug>

QTreeJsonWidget::QTreeJsonWidget(QObject *parent) : QObject(parent)
{
}

QTreeWidgetItem * QTreeJsonWidget::load(const QJsonValue& value, QTreeWidgetItem* parent) {
    QTreeWidgetItem * rootItem = new QTreeWidgetItem(parent);
    rootItem->setText(0, "root");

    if ( value.isObject()) {
        //Get all QJsonValue childs
        for (QString key : value.toObject().keys()) {
            QJsonValue v = value.toObject().value(key);
            QTreeWidgetItem * child = load(v,rootItem);
            child->setText(0, key);
            rootItem->addChild(child);
        }
    } else if ( value.isArray()) {
        //Get all QJsonValue childs
        int index = 0;
        for (QJsonValue v : value.toArray()){
            QTreeWidgetItem * child = load(v,rootItem);
            child->setText(0, QString::number(index));
            rootItem->addChild(child);
            ++index;
        }
    } else {
        rootItem->setText(1, value.toVariant().toString());
    }
    return rootItem;
}

bool QTreeJsonWidget::loadJson(const QByteArray &json)
{
    mDocument = QJsonDocument::fromJson(json);
    if (!mDocument.isNull()) {
        if (mDocument.isArray()) {
            mRootItem = load(QJsonValue(mDocument.array()), 0);
        } else {
            mRootItem = load(QJsonValue(mDocument.object()), 0);
        }
        return true;
    }
    qDebug() << "cannot load json";
    return false;
}

void QTreeJsonWidget::changeRow() {
    if (tree->currentColumn() < 0) {
        return;
    }

    QDialog *d = new QDialog();
    change_sett_wind = new ChangeSettingsWindow(d);
    connect(change_sett_wind, SIGNAL(pressedOkButton()), d, SLOT(close()));
    connect(change_sett_wind, SIGNAL(pressedCancelButton()), d, SLOT(close()));
    d->exec();

    if(!change_sett_wind->isCancel()) {
        QString key = change_sett_wind->getKey();
        QString value = change_sett_wind->getValue();
        tree->currentItem()->setText(0, key);
        if (tree->currentItem()->child(0)) {
            return;
        }
        tree->currentItem()->setText(1, value);
    }
}

void QTreeJsonWidget::addRow() { //добавить запись в ту же строку, что и элемент
    QTreeWidgetItem *parent = tree->currentItem()->parent();
    if (parent) {
        QTreeWidgetItem * new_item = new QTreeWidgetItem();
        new_item->setText(0, "New");
        new_item->setText(1, "");
        parent->insertChild(0, new_item);
    }
}

void QTreeJsonWidget::addChildRow() { //добавить дочернюю запись
    QTreeWidgetItem *current_item = tree->currentItem();
    if (current_item->text(1) != "") {
        return;
    }
    if (current_item) {
        QTreeWidgetItem * new_item = new QTreeWidgetItem();
        new_item->setText(0, "New");
        new_item->setText(1, "");
        current_item->addChild(new_item);
    }
}

void QTreeJsonWidget::delRow() {
    QTreeWidgetItem * it = tree->currentItem();
    delete it;
}

QTreeWidget QTreeJsonWidget::generateTree(std::string json) {
    tree = new QTreeWidget();
    if (loadJson(QByteArray::fromStdString(json))) {
        tree->addTopLevelItem(mRootItem);
        tree->setColumnCount(2);
        tree->setColumnWidth(0, 230);
        tree->headerItem()->setText(0, "Key");
        tree->headerItem()->setText(1, "Value");

        //connect(ui->actionChangeRow, SIGNAL(triggered()), this, SLOT(changeRow()));
        //connect(ui->actionAdd, SIGNAL(triggered()), this, SLOT(addRow()));
        //connect(ui->actionDel, SIGNAL(triggered()), this, SLOT(delRow()));
        //connect(ui->actionAddChildRow, SIGNAL(triggered()), this, SLOT(addChildRow()));

        return tree;
    } else {
        qDebug() << "Noup";
    }
}
