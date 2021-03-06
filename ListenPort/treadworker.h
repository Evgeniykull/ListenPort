#ifndef TREADWORKER_H
#define TREADWORKER_H

#include <QObject>
#include <QSerialPort>

class treadWorker : public QObject
{
    Q_OBJECT
public:
    treadWorker();
    void setTransferParams(QByteArray data,
                           QSerialPort *device_port,
                           unsigned char dev_addres,
                           int dev_byteOnPackage);

signals:
    void finished(QByteArray);

public slots:
    void transferData();
    void stop();

public:
    QByteArray *_resievedData;
    QByteArray aaa;

private:
    QByteArray _data;
    QSerialPort *port;
    unsigned char buff[255];
    unsigned char errnum;  // Количество ошибочных обменов
    unsigned char errcode; // Код ошибки обмена
    unsigned char answc;   // Код ответа
    unsigned char addres;
    int byteOnPackage;

    void putPortData(QByteArray tr_data);
    QByteArray getPortData(char prebyte,int len);
    QByteArray getPortDataOnly(int len);
};

#endif // TREADWORKER_H
