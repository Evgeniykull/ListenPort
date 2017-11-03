#include "treadworker.h"
#include "utils/catalogswrither.h"
#include <QTextCodec>
#include <QMessageBox>

treadWorker::treadWorker()
{
}

void treadWorker::setTransferParams(QByteArray data, QByteArray &resievedData, QSerialPort *device_port, unsigned char dev_addres, int dev_byteOnPackage) {
    _data = data;
    _resievedData = resievedData;
    port = device_port;
    addres = dev_addres;
    byteOnPackage = dev_byteOnPackage;
}

void treadWorker::transferData() {
    QTextCodec *codec1 = QTextCodec::codecForName( "CP1251" );
    _data = codec1->fromUnicode(QString(_data));

    //Подготавливаем данные
    QByteArray data = _data;
    int data_len = data.length();
    if (data_len == 0) {
        _resievedData = "";
        emit finished();
        return;
    }

    int iter;
    //unsigned char addres = ui->leAddres->text().toInt();
    unsigned char tranz_num = 0;
    uint data_pos = 0;
    uint max_pkg_len;
    unsigned char answ_kadr;
    (data_len < 16) ? max_pkg_len = 16 : max_pkg_len = byteOnPackage;

    while (data_len) {
        buff[0] = 0xB5;
        buff[1] = addres;
        if (data_len <= max_pkg_len) { // Все данные помещаются в один пакет - он или единственный, или завершающий
            buff[2] = data_len;
            buff[3] = (data_pos == 0) ? 0x00 : 0x03;
        } else {
            buff[2] = max_pkg_len;
            buff[3] = (data_pos == 0) ? 0x01 : 0x02;
        }
        buff[4] = tranz_num;
        for(iter = 0; iter < buff[2]; iter++) {
            buff[iter + 5] = data[iter + data_pos];
        }
        data_len -= iter;
        Calc_hCRC(buff, iter + 5);

        //Цикл обмена одним файлом
        errnum = 0;
        errcode = 0x88;
        answc = 0;
        QByteArray rd_data;
        do {
            rd_data.clear();
            //отправка
            int size = buff[2] + 7;
            QByteArray tr_data = QByteArray((char *)buff, size);
            port->write(tr_data);
            if (port->waitForBytesWritten(200)) {
                //прием файла
                if (port->waitForReadyRead(100)) {
                    rd_data.append(port->read(8));
                    if (rd_data.length() < 8) {
                        port->waitForReadyRead(100);
                        rd_data.append(port->read(8 - rd_data.length()));
                    }
                }
            }

            if (rd_data.length()) {
                if (Calc_hCRC((unsigned char*)rd_data.data(), 6)) {
                    if ((rd_data[0] == 0x3A)&&
                        (rd_data[1] == tr_data[1])&&
                        (rd_data[2] == tr_data[2])&&
                        (rd_data[3] == tr_data[3])&&
                        (rd_data[4] == tr_data[4])) {
                        //Ответный кадр верный
                        answc = rd_data[5]&0xF9;
                        //Проверка на правильность принятия
                        (answc == 0x50 || answc == 0x51) ? errcode = 0 : errcode = rd_data[5];
                    }
                }
            }
            if (errcode) errnum++;
            if (errcode >= 0x80 && errcode != 0x88) break;
        } while (errnum < 3 && errcode != 0);
        if (errcode) {
            QMessageBox::information(0, QObject::tr("Can not write to device"),
                    "Обмен завершен с ошибкой 0x" + QString("%1").arg(errcode, 0, 16).toUpper());
            _resievedData = "";
            emit finished();
            return;
        }
        //Обмен завершен успешно
        tranz_num++;
    }

    //Инициируем получение данных
    unsigned char keysign[7]; // Ключ для проверки кадра на повтор
    unsigned char answ[8];
    int answlen;
    errnum = 0;
    data_pos = 0;
    data_len = 0;
    for (int n = 0; n < 7; n++) keysign[n] = 0;
    for (int n = 0; n < 8; n++) answ[n] = 0;
    answlen = 0;
    unsigned char answer;
    bool not_end = true;

    do {
        errcode = 0;
        //Формируем запрос данных или подтверждение приема пакета
        if (data_pos == 0 && answlen == 0) {
            buff[0]=0xB5;
            buff[1]=addres;
            buff[2]=0;
            buff[3]=0x0F;
            buff[4]=(tranz_num + 1);
            Calc_hCRC(buff,5);
            QByteArray tr_data = QByteArray((char *)buff, 7);
            //see writeData
            port->write(tr_data);
            port->waitForBytesWritten(20);
        } else {
            // По какой-то причине нужно повторить запрос - ответ не пришел/поврежден или продолжение отправки
            port->write((char*)answ, answlen);
            port->waitForBytesWritten(20);
        }
        // Ожидаем прихода следующего кадра
        QByteArray rd_data;
        if (port->waitForReadyRead(100)) {
            rd_data.append(port->read(5));
            if (rd_data.length() != 5) {
                port->waitForReadyRead(100);
                rd_data.append(port->read(5 - rd_data.length()));
            }
        }
        if (rd_data[0] == 0xB5) {
            data_len = abs(rd_data[2]);
            QByteArray rd_data1;
            port->waitForReadyRead(100);
            delay(1);
            rd_data1.append(port->read(data_len + 2));
            //qDebug() << "data_len: " << data_len;
            //qDebug() << "rd_data1: " << rd_data1;
            if (rd_data1.length() != data_len + 2) {
                port->waitForReadyRead(100);
                rd_data1.append(port->read(data_len + 2 - rd_data1.length()));
                //qDebug() << "rd_data1_con: " << rd_data1;
            }
            if (rd_data1.length() == data_len + 2) {
                //Кадр нормальной длинны получен
                rd_data += rd_data1;
                if (Calc_hCRC((unsigned char*)rd_data.data(), data_len + 5)) {
                    if (rd_data[1] == addres) {
                        // Проверяем кадр на повтор
                        int nn;
                        for (int n = 0; n < 5; n++) if (keysign[n] != rd_data[n]) nn = 1;
                        if (rd_data[data_len + 5] != keysign[5] || rd_data[data_len + 6] != keysign[6]) nn = 1;
                        if (nn) {
                            // Запоминаем пришедший кадр - он прошел проверки на нормальность
                            for (int n = 0; n < 5; n++) keysign[n] = rd_data[n];
                            keysign[5] = rd_data[data_len + 5];
                            keysign[6] = rd_data[data_len + 6];
                            // Анализируем что за кадр пришел
                            answ_kadr = rd_data[3];
                            switch (answ_kadr) {
                                case (0x80): // Прием за 1 раз
                                    for (uint n = 0; n < data_len; n++) data[n] = rd_data[5 + n];
                                    answer = 0x51;
                                    not_end = false;
                                    break;
                                case (0x81): // Начало приема сообщения
                                    data_pos ? answer = 0x57 : answer = 0x50;
                                    for (uint n = 0; n < data_len; n++) data[data_pos++] = rd_data[5 + n];
                                    break;
                                case (0x82): // Продолжение приема сообщения
                                    if (data_pos) {
                                        answer = 0x50;
                                        for (uint n = 0; n < data_len; n++) data[data_pos++] = rd_data[5 + n];
                                    } else {
                                        answer = 0x84;
                                        data_len = -1;
                                    }
                                    break;
                                case (0x83):  // Окончание приема сообщения
                                    if (data_pos) {
                                        answer=0x51;
                                        for (uint n = 0;n < data_len; n++) data[data_pos++]=rd_data[5 + n];
                                            data_len = data_pos;
                                    } else {
                                        answer = 0x84;
                                        data_len = -1;
                                    }
                                    not_end = false;
                                    break;
                                default: answer = 0x83;
                            }
                            answ[0]=0x3A;
                            answ[1]=rd_data[1];
                            answ[2]=rd_data[2];
                            answ[3]=rd_data[3];
                            answ[4]=rd_data[4];
                            answ[5]=answer;
                            Calc_hCRC(answ,6);
                            answlen=8;
                        }
                        // Отправляем ответ, или сформированный или предыдущий
                        // Ответ отправляется в начале цикла
                    } // Иначе - не ошибка, а чужой пакет
                } else errcode=0x88;
            } else errcode=0x88;
        } else errcode=0x88;
        if (errcode) errnum++;
    } while ((errnum < 3) && (data_len >= 0) && not_end);
    if (answer == 0x51 || answer == 0x57) {
        port->write((char*)answ);
        port->waitForBytesWritten(20);
    }
    if (data_len > 0) {
        data[data_len++] = 0;
        QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
        QString data_1 = codec->toUnicode(data);  //то, что пришло в формате читаемой строки
        _resievedData = QByteArray(data_1.toStdString().c_str());
        emit finished();
        return;
    }

    /*QMessageBox::information(0, QObject::tr("Write to device"),
            QObject::tr("Ошибка обмена"));*/
    _resievedData = "";
    emit finished();
}

void treadWorker::stop() {

}
