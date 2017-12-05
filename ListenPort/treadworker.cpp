#include "treadworker.h"
#include "utils/catalogswrither.h"
#include <QTextCodec>

treadWorker::treadWorker()
{
}

void treadWorker::setTransferParams(QByteArray data, QSerialPort *device_port, unsigned char dev_addres, int dev_byteOnPackage) {
    _data = data;
    port = device_port;
    addres = dev_addres;
    byteOnPackage = dev_byteOnPackage;
}

QByteArray treadWorker::getPortDataOnly(int len) {
  QByteArray qba;
  int tosumm=0;  // Счетчик общего времени ожидания
  qba.clear();
  tosumm=0;
  while ((qba.length()<len)&&(tosumm<40)) {    // Здесь - время межбайтового таймаута
    int llen=qba.length();
    int crn=len-llen;
    QByteArray readed;
    readed=port->read(crn);
    if (readed.length()==0) {
      port->waitForReadyRead(5);  // Ожидаем - если ничего не пришло ещё
      tosumm+=5;
    } else {
      qba.append(readed);
      tosumm=0;
    }
  }
  return qba;
}

QByteArray treadWorker::getPortData(char prebyte,int len) {
  QByteArray qba;
  int tosumm=0;  // Счетчик общего времени ожидания
  qba.clear();
// Ожидаем начальный байт пакета
  do {
    port->waitForReadyRead(10);
    qba.append(port->read(1));
// Считываем по одному байту до тех пор, пока не найдем преамбулу
// До преамбулы может быть мусор - его откидываем
    if ((qba.length()<1)?(1):(qba[0]!=prebyte)) {
      qba.clear();
      tosumm+=10;
    }
  } while ((qba.length()==0)&&(tosumm<200)); // Здесь - время в миллисекундах максимального ожидания ответа
  if (qba.length()!=0) { // Преамбула пришла - иначе ответа нет
    tosumm=0;
    while ((qba.length()<len)&&(tosumm<50)) {    // Здесь - время межбайтового таймаута
      int llen=qba.length();
      int crn=len-llen;
      QByteArray readed;
      readed=port->read(crn);
      if (readed.length()==0) {
        port->waitForReadyRead(5);
        tosumm+=5;
      } else {
        qba.append(readed);
        tosumm=0;
      }
    }
  }
  return qba;
}

void treadWorker::putPortData(QByteArray tr_data) {
  port->flush();
  port->write(tr_data);
}

void treadWorker::transferData()
{
// Перекодировали в CP1251 - хорошо
    QTextCodec *codec1 = QTextCodec::codecForName( "CP1251" );
    _data = codec1->fromUnicode(QString(_data));
//Подготавливаем данные - при пустой строке возвращаем пустую строку - логично
    QByteArray data = _data;
    int data_len = data.length();
    if (data_len == 0) {
        //*_resievedData = "error 1";
        aaa = "error 1";
        emit finished(aaa);
        return;
    }
// Объявления переменных... надеюсь, современные компиляторы позволяют их делать в середине. Мне мой не позволяет
    int iter;
    unsigned char tranz_num = 0;
    uint data_pos = 0;
    uint max_pkg_len;
    unsigned char answ_kadr;
// fromAlex - Вообще назначение данной конструкции не очень понятно - думаю, логичнее провреять byteOnPackage
    if (byteOnPackage<16) max_pkg_len=16; else max_pkg_len = byteOnPackage;
//
    while (data_len) {
// Формируем один кадр
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
//Цикл обмена одним кадром
        errnum = 0;
        errcode = 0x88;
        answc = 0;
        QByteArray rd_data;
        do {
            rd_data.clear();
            //отправка
            int size = buff[2] + 7;
            QByteArray tr_data = QByteArray((char *)buff, size);
// Отправка-прием
            putPortData(tr_data);
            rd_data=getPortData(0x3A,8);

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
            //*_resievedData = "error 2";
            aaa = "error 2";
            emit finished(aaa);
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
            putPortData(tr_data);
        } else {
            // По какой-то причине нужно повторить запрос - ответ не пришел/поврежден или продолжение отправки
            putPortData(QByteArray((char*)answ, answlen));
            //*_resievedData = "\n Resive 1 \n";
            aaa = "\n Resive 1 \n";
        }
        // Ожидаем прихода следующего кадра
        QByteArray rd_data;
        rd_data=getPortData(0xB5,5);
        if (rd_data[0] == 0xB5) {
            data_len = abs(rd_data[2]);
            QByteArray rd_data1;
            rd_data1=getPortDataOnly(data_len+2);
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
                                    //*_resievedData = "Start \n";
                                    data_pos ? answer = 0x57 : answer = 0x50;
                                    for (uint n = 0; n < data_len; n++) data[data_pos++] = rd_data[5 + n];
                                    break;
                                case (0x82): // Продолжение приема сообщения
                                    //*_resievedData = "Cont \n";
                                    if (data_pos) {
                                        answer = 0x50;
                                        for (uint n = 0; n < data_len; n++) data[data_pos++] = rd_data[5 + n];
                                    } else {
                                        answer = 0x84;
                                        data_len = -1;
                                    }
                                    break;
                                case (0x83):  // Окончание приема сообщения
                                    //*_resievedData = "End \n";
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
                                default:
                                    answer = 0x83;
                                    //*_resievedData = "Default \n";
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
        putPortData(QByteArray((char*)answ, answlen));
    }
    if (data_len > 0) {
        data[data_len++] = 0;
        QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
        QString data_1 = codec->toUnicode(data);  //то, что пришло в формате читаемой строки
        //*_resievedData = QByteArray(data_1.toStdString().c_str());
        aaa = QByteArray(data_1.toStdString().c_str());
        emit finished(aaa);
        return;
    }
    //*_resievedData = QByteArray("3!!") + errcode;
    aaa = QByteArray("3!!") + errcode;
    emit finished(aaa);
}

void treadWorker::stop() {

}
