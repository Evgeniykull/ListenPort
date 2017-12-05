#ifndef LISTENPORT_H
#define LISTENPORT_H

#include <QMainWindow>
#include <QString>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTreeWidgetItem>
#include <QJsonDocument>
#include <QMap>
#include <QMessageBox>
#include "changesettingswindow.h"
#include "utils/catalogswrither.h"

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

class ListenPort : public QMainWindow
{
    Q_OBJECT

public:
    explicit ListenPort(QWidget *parent = 0);
    ~ListenPort();
    Settings SettingsPort;
    void waitMessageShow(QByteArray);

private slots:
    void getPortsInfo();
    void changePort(QString name);
    void openPort();
    void closePort();
    // Функция получения данных из порта с учетом таймаутов
    // Возвращает массив с данными нужной длины или пустой массив
    // первый параметр - начальный байт пакета
    // второй параметр - общее число байт пакета
    QByteArray getPortData(char,int);
    // Функция получения данных из порта с учетом таймаутов
    // Возвращает массив с данными нужной длины или пустой массив
    // параметр - общее число байт пакета
    QByteArray getPortDataOnly(int);
    // Выдает данные в порт
    void putPortData(QByteArray tr_data);
    // Обмен одной командой и получение ответа
    QByteArray writeData(QByteArray text);
    QByteArray analizeData(QByteArray data);
    void writePortSettings();//доработать
    void checkCustomBaudRatePolicy(int idx);

    void onSettingsClick();
    void onParamsClick();
    void onChangeParamsClick();

    QString dataToJson(QByteArray data);
    void buildJsonTree(QString json);
    void changeRowTree();
    void addRowTree();
    void delRowTree();
    void addChildRowTree();

    void prepareMenu(const QPoint & pos);

    void saveInFile();
    void readFromFile();

    void writeCatalog();
    void getCatalog();
    void writeFileToDevice();
    void readFileFromDevice();
    void writeChangeToDevice();

    void updateSettings();
    void updateInfo();

    void changeViewMod();
    void changePrecision();
    void onAccessClick();
    void onChangeAccessClick();
    void onAccessUpdateClick();

    void endTransmitData(QByteArray);

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
    bool transfer_data = false; //если true, то запрет на передачу
    int tab = 1;
    int byteOnPackage;

    void addValueToSettings();
    void setSavingParams();
    void getPortSettingsFromFile();

    //для вывода json в графику (вынести в отдельный класс)
    ChangeSettingsWindow * change_sett_wind;
    bool loadJson(const QString &json);
    QTreeWidgetItem * load(const QJsonValue& value, QTreeWidgetItem* parent);
    QTreeWidgetItem * mParent;
    QTreeWidgetItem * mRootItem;
    QJsonDocument mDocument;
    void generateTree(std::string json);

    QMap<QString, QString> changed_key;

    const QByteArray SETTINGS = "Установки";
    const QByteArray INFO = "Состояние";
    bool treeViewMod = false;
    int precision = 2;

    QByteArray answerFromThread;
    QMessageBox msgBx;
};

#endif // LISTENPORT_H
