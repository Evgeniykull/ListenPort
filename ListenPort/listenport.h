#ifndef LISTENPORT_H
#define LISTENPORT_H

#include <vector>
#include <QMainWindow>
#include <QString>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTreeWidgetItem>
#include <QJsonDocument>
#include "changesettingswindow.h"


namespace Ui {
class ListenPort;
}

struct Settings {
    QString name;
    qint32 baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::Parity parity;
    QSerialPort::StopBits stopBits;
    QSerialPort::FlowControl flowControl;
};


//using byte = unsigned char;

class ListenPort : public QMainWindow
{
    Q_OBJECT

public:
    explicit ListenPort(QWidget *parent = 0);
    ~ListenPort();
    Settings SettingsPort;

private slots:
    void getPortsInfo();
    void changePort(QString name);
    void openPort();
    void closePort();
    void writeData(QString text);
    void writePortSettings();//доработать
    void checkCustomBaudRatePolicy(int idx);

    void onSettingsClick(bool enab);
    void onParamsClick();
    void onChangeParamsClick();

    void dataToJson(QByteArray data);
    void buildJsonTree(QString json);
    void changeRowTree();
    void addRowTree();
    void delRowTree();
    void addChildRowTree();

    void prepareMenu(const QPoint & pos);

    void saveInFile();
    void readFromFile();

    void writeCatalog();

private:
    Ui::ListenPort *ui;
    QSerialPort *port;
    QStringList port_names;
    QStringList port_discriptions;
    QStringList port_manufacturers;
    unsigned char buff[255];
    unsigned char errnum;  // Количество ошибочных обменов
    unsigned char errcode; // Код ошибки обмена
    unsigned char answc;   // Код ответа
    int last_index;
    bool transfer_data = false; //если true, то запрет на передачу

    void addValueToSettings();
    void setSavingParams();

    //для вывода json в графику (вынести в отдельный класс)
    ChangeSettingsWindow * change_sett_wind;
    bool loadJson(const QString &json);
    QTreeWidgetItem * load(const QJsonValue& value, QTreeWidgetItem* parent);
    QTreeWidgetItem * mParent;
    QTreeWidgetItem * mRootItem;
    QJsonDocument mDocument;
    void generateTree(std::string json);
    int tab;

};

#endif // LISTENPORT_H
