#include "listenport.h"
#include "ui_listenport.h"
#include "jsonconvertor.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QTabWidget>
#include <QTextCodec>
#include <QTime>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMenu>
#include <QFileDialog>


ListenPort::ListenPort(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ListenPort)
{
    ui->setupUi(this);
    ui->leAddres->setValidator(new QIntValidator(0, 100000, this));
    port = new QSerialPort(this);

    last_index = 1;

    addValueToSettings();
    connect(ui->cbBaudRate, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCustomBaudRatePolicy(int)));

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actSettings, SIGNAL(triggered(bool)), this, SLOT(onSettingsClick(bool)));
    connect(ui->actParams, SIGNAL(triggered()), this, SLOT(onParamsClick()));
    connect(ui->actChangeParams, SIGNAL(triggered()), this, SLOT(onChangeParamsClick()));

    connect(ui->cbPortName, SIGNAL(currentTextChanged(QString)), this, SLOT(changePort(QString)));

    connect(ui->actionOpen_port, SIGNAL(triggered()), this, SLOT(openPort()));
    connect(ui->actionClose_port, SIGNAL(triggered()), this, SLOT(closePort()));
    connect(ui->pbConfigurate, SIGNAL(clicked()), this, SLOT(writePortSettings()));
    getPortsInfo();
    setSavingParams();

    connect(ui->actionChangeRow, SIGNAL(triggered()), this, SLOT(changeRowTree()));
    connect(ui->actionAddRow, SIGNAL(triggered()), this, SLOT(addRowTree()));
    connect(ui->actionAddChildrenRow, SIGNAL(triggered()), this, SLOT(addChildRowTree()));
    connect(ui->actionDeleteRow, SIGNAL(triggered()), this, SLOT(delRowTree()));

    ui->tabWidget->tabBar()->hide(); //для скрытия табов
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu); //для отображения контекстного меню в деревьях
    ui->treeWidget_2->setContextMenuPolicy(Qt::CustomContextMenu); //для отображения контекстного меню в деревьях
    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(prepareMenu(QPoint)));
    connect(ui->treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(changeRowTree()));

    connect(ui->actSave, SIGNAL(triggered()), this, SLOT(saveInFile())); //save in file
    connect(ui->actReadFormFile, SIGNAL(triggered()), this, SLOT(readFromFile())); //read from file
    connect(ui->actWriteCatalog, SIGNAL(triggered()), this, SLOT(writeCatalog()));
}

ListenPort::~ListenPort()
{
    delete ui;
}

void ListenPort::writeCatalog() {
    catalogswrither catalog;
    catalog.writeCatalogToFile();
}

void ListenPort::saveInFile() {
    QString fileName = QFileDialog::getSaveFileName(this,
            tr("Save settings"), "",
            tr("All Files (*)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Unable to open file"),
            file.errorString());
        return;
    }
    JsonConvertor *convertor = new JsonConvertor();
    QString data_string = convertor->dataByTree(ui->treeWidget);
    QByteArray data_string_array = QByteArray((char*)data_string.toStdString().c_str());
    file.write(data_string_array);
    file.close();
}

void ListenPort::readFromFile() { //проблемы с кодировкой
    QString fileName = QFileDialog::getOpenFileName(this,
            tr("Open settings"), "",
            tr("All Files (*)"));
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Unable to open file"),
            file.errorString());
        return;
    }
    QByteArray file_data = file.readAll();
    file_data.insert(0, "answ get ");
    dataToJson(file_data);
}

void ListenPort::prepareMenu(const QPoint &pos) {
    QTreeWidget * tree = ui->treeWidget;
    QTreeWidgetItem *nd = tree->itemAt( pos );

    QMenu menu(this);
    menu.addAction(ui->actionChangeRow);
    menu.addAction(ui->actionAddRow);
    menu.addAction(ui->actionAddChildrenRow);
    menu.addAction(ui->actionDeleteRow);

    QPoint pt(pos);
    menu.exec(tree->mapToGlobal(pos));
}

void ListenPort::changeRowTree() {
    if (ui->treeWidget->currentColumn() < 0) {
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
        ui->treeWidget->currentItem()->setText(0, key);
        if (ui->treeWidget->currentItem()->child(0)) {
            return;
        }
        ui->treeWidget->currentItem()->setText(1, value);
    }
}

void ListenPort::addRowTree() { //добавляет запись в ту же строку, что и элемент
    QTreeWidgetItem *parent = ui->treeWidget->currentItem()->parent();
    if (parent) {
        QTreeWidgetItem * new_item = new QTreeWidgetItem();
        new_item->setText(0, "New");
        new_item->setText(1, "");
        parent->insertChild(0, new_item);
    }
}

void ListenPort::addChildRowTree() { //добавляет дочернюю запись
    QTreeWidgetItem *current_item = ui->treeWidget->currentItem();
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

void ListenPort::delRowTree() {
    QTreeWidgetItem * it = ui->treeWidget->currentItem();
    delete it;
}

QTreeWidgetItem * ListenPort::load(const QJsonValue& value, QTreeWidgetItem* parent) {
    QTreeWidgetItem * rootItem = new QTreeWidgetItem(parent);
    if (tab == 1) {
        rootItem->setText(0, "Settings");
    } else {
        rootItem->setText(0, "Info");
    }

    if ( value.isObject()) {
        foreach (QString key, value.toObject().keys()) {
            QJsonValue v = value.toObject().value(key);
            QTreeWidgetItem * child = load(v,rootItem);
            child->setText(0, key);
            rootItem->addChild(child);
        }
    } else if ( value.isArray()) {
        int index = 0;
        foreach (QJsonValue v, value.toArray()) {
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

bool ListenPort::loadJson(const QString &json)
{
    mDocument = QJsonDocument::fromJson(json.toUtf8());
    if (!mDocument.isNull()) {
        if (mDocument.isArray()) {
            mRootItem = load(QJsonValue(mDocument.array()), 0);
        } else {
            mRootItem = load(QJsonValue(mDocument.object()), 0);
        }
        return true;
    }
    QMessageBox msgBox;
    msgBox.setText("Cannot load json to tree.");
    msgBox.exec();
    return false;
}

void ListenPort::buildJsonTree(QString json) {
    QTreeWidget *wid;
    (tab == 1) ? wid = ui->treeWidget :
        wid = ui->treeWidget_2;

    wid->clear();
    if (loadJson(json)) {
        wid->addTopLevelItem(mRootItem);
        wid->setColumnWidth(0, 230);
        wid->headerItem()->setText(0, "Key");
        wid->headerItem()->setText(1, "Value");
    } else {
        QMessageBox msgBox;
        msgBox.setText("Cannot convert json to tree.");
        msgBox.exec();
    }
}

void ListenPort::onParamsClick() {
    last_index = 2;
    ui->tabWidget->setCurrentIndex(2);
    if (port->isOpen()) {
        QString lbl_settings = "get Info";
        tab = 2;
        writeData(lbl_settings);
    }
}

void ListenPort::onChangeParamsClick() {
    last_index = 1;
    ui->tabWidget->setCurrentIndex(1);
    if (port->isOpen()) {
        QString lbl_settings = "get Settings";
        tab = 1;
        writeData(lbl_settings);
    }
}

void ListenPort::onSettingsClick(bool enab) {
    if (enab) {
        getPortsInfo();
        ui->tabWidget->setCurrentIndex(0);
        return;
    }
    ui->tabWidget->setCurrentIndex(last_index);
}

void ListenPort::setSavingParams() { //установка сохраненных параметров
    QFile file("settings.ini");
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList list = line.split("=", QString::SkipEmptyParts);
        if (list.length() == 1) continue;
        if (list[0] == "ComPort" && ui->cbPortName->findText(list[1]) + 1) {
            ui->cbPortName->setCurrentIndex(ui->cbPortName->findText(list[1]));
            continue;
        }
        if (list[0] == "BaudRate" && ui->cbBaudRate->findText(list[1]) + 1) {
            ui->cbBaudRate->setCurrentIndex(ui->cbBaudRate->findText(list[1]));
            continue;
        }
        if (list[0] == "DataBits" && ui->cbDataBist->findText(list[1]) + 1) {
            ui->cbDataBist->setCurrentIndex(ui->cbDataBist->findText(list[1]));
            continue;
        }
        if (list[0] == "Pairity" && ui->cbPairity->findText(list[1]) + 1) {
            ui->cbPairity->setCurrentIndex(ui->cbPairity->findText(list[1]));
            continue;
        }
        if (list[0] == "StopBits" && ui->cbStopBits->findText(list[1]) + 1) {
            ui->cbStopBits->setCurrentIndex(ui->cbStopBits->findText(list[1]));
            continue;
        }
        if (list[0] == "FlowControl" && ui->cbFlowControl->findText(list[1]) + 1) {
            ui->cbFlowControl->setCurrentIndex(ui->cbFlowControl->findText(list[1]));
            continue;
        }
        if (list[0] == "Addres") {
            ui->leAddres->setText(list[1]);
            continue;
        }
    }
    file.close();
}

void ListenPort::addValueToSettings() {
    //data bits
    ui->cbDataBist->addItem(QLatin1String("5"), QSerialPort::Data5);
    ui->cbDataBist->addItem(QLatin1String("6"), QSerialPort::Data6);
    ui->cbDataBist->addItem(QLatin1String("7"), QSerialPort::Data7);
    ui->cbDataBist->addItem(QLatin1String("8"), QSerialPort::Data8);
    ui->cbDataBist->setCurrentIndex(3);
    //baud rate
    ui->cbBaudRate->addItem(QLatin1String("9600"), QSerialPort::Baud9600);
    ui->cbBaudRate->addItem(QLatin1String("19200"), QSerialPort::Baud19200);
    ui->cbBaudRate->addItem(QLatin1String("38400"), QSerialPort::Baud38400);
    ui->cbBaudRate->addItem(QLatin1String("115200"), QSerialPort::Baud115200);
    ui->cbBaudRate->addItem(QLatin1String("Custom"));
    //fill party
    ui->cbPairity->addItem(QLatin1String("None"), QSerialPort::NoParity);
    ui->cbPairity->addItem(QLatin1String("Even"), QSerialPort::EvenParity);
    ui->cbPairity->addItem(QLatin1String("Odd"), QSerialPort::OddParity);
    ui->cbPairity->addItem(QLatin1String("Mark"), QSerialPort::MarkParity);
    ui->cbPairity->addItem(QLatin1String("Space"), QSerialPort::SpaceParity);
    //fill stop bits
    ui->cbStopBits->addItem(QLatin1String("1"), QSerialPort::OneStop);
    #ifdef Q_OS_WIN
    ui->cbStopBits->addItem(QLatin1String("1.5"), QSerialPort::OneAndHalfStop);
    #endif
    ui->cbStopBits->addItem(QLatin1String("2"), QSerialPort::TwoStop);
    //fill flow control
    ui->cbFlowControl->addItem(QLatin1String("None"), QSerialPort::NoFlowControl);
    ui->cbFlowControl->addItem(QLatin1String("RTS/CTS"), QSerialPort::HardwareControl);
    ui->cbFlowControl->addItem(QLatin1String("XON/XOFF"), QSerialPort::SoftwareControl);
}

void ListenPort::checkCustomBaudRatePolicy(int idx) {
    bool isCustomBaudRate = !ui->cbBaudRate->itemData(idx).isValid();
    ui->cbBaudRate->setEditable(isCustomBaudRate);
    if (isCustomBaudRate) {
        ui->cbBaudRate->clearEditText();
    }
}

void ListenPort::writePortSettings() {
    SettingsPort.name = ui->cbPortName->itemText(ui->cbPortName->currentIndex());
    SettingsPort.baudRate = (QSerialPort::BaudRate) ui->cbBaudRate->itemText(ui->cbBaudRate->currentIndex()).toInt();
    SettingsPort.dataBits = (QSerialPort::DataBits) ui->cbDataBist->itemText(ui->cbDataBist->currentIndex()).toInt();
    SettingsPort.parity = (QSerialPort::Parity) ui->cbPairity->itemText(ui->cbPairity->currentIndex()).toInt();
    SettingsPort.stopBits = (QSerialPort::StopBits) ui->cbStopBits->itemText(ui->cbStopBits->currentIndex()).toInt();
    SettingsPort.flowControl = (QSerialPort::FlowControl) ui->cbFlowControl->itemText(ui->cbFlowControl->currentIndex()).toInt();

    QFile file("settings.ini");
    file.open(QIODevice::ReadWrite);

    QTextStream out(&file);
    out << "[Baseparams]" << "\n";
    out << "ComPort=" << SettingsPort.name << "\n";
    out << "BaudRate=" << ui->cbBaudRate->itemText(ui->cbBaudRate->currentIndex()) << "\n";
    out << "DataBits=" << ui->cbDataBist->itemText(ui->cbDataBist->currentIndex()) << "\n";
    out << "Pairity=" << ui->cbPairity->itemText(ui->cbPairity->currentIndex()) << "\n";
    out << "StopBits=" << ui->cbStopBits->itemText(ui->cbStopBits->currentIndex()) << "\n";
    out << "FlowControl=" << ui->cbFlowControl->itemText(ui->cbFlowControl->currentIndex()) << "\n";
    out << "Addres=" << ui->leAddres->text() << "\n";

    file.close();
}

int Calc_hCRC(unsigned char * bf, uint len) {
    uint n;
    ushort crc = 0xFFFF;
    for (n = 0; n < len; n++) {
        crc += ((ushort)bf[n])*44111;
        crc = crc ^ (crc >> 8);
    }

    if ((bf[n] == (crc&0xFF)) && (bf[n + 1] == ((crc>>8)&0xFF)))
        return 1;

    bf[n]=crc&0xFF;
    bf[n+1]=(crc>>8)&0xFF;
    return 0;
}

void delay(int num) //для ожидания, пока идет чтение
{
    QTime dieTime= QTime::currentTime().addSecs(num);
    while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void ListenPort::writeData(QString text)
{
    if (transfer_data) {
        return;
    } else {
        transfer_data = true;
    }

    //Подготавливаем данные
    QByteArray data = text.toLatin1();
    int data_len = data.length();
    if (data_len == 0) {
        ui->teInfo->setText("No data to send");
        return;
    }

    int iter;
    unsigned char addres = ui->leAddres->text().toInt();
    unsigned char tranz_num = 0;
    uint data_pos = 0;
    uint max_pkg_len;
    unsigned char answ_kadr;
    (data_len < 16) ? max_pkg_len = 16 : max_pkg_len = 255;

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
            if (port->waitForBytesWritten(20)) {
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
            (tab == 1) ? ui->teInfo->setText("Обмен завершен с ошибкой 0x" + QString("%1").arg(errcode, 0, 16).toUpper()) :
                ui->teInfo_2->setText("Обмен завершен с ошибкой 0x" + QString("%1").arg(errcode, 0, 16).toUpper());
            //break;
            transfer_data = false;
            return;
        }
        //Обмен завершен успешно
        tranz_num++;
        (tab == 1) ? ui->teInfo->setText("Обмен завершен успешно") :
                ui->teInfo_2->setText("Обмен завершен успешно");
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
            qDebug() << "data_len: " << data_len;
            qDebug() << "rd_data1: " << rd_data1;
            if (rd_data1.length() != data_len + 2) {
                port->waitForReadyRead(100);
                rd_data1.append(port->read(data_len + 2 - rd_data1.length()));
                qDebug() << "rd_data1_con: " << rd_data1;
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
        transfer_data = false;
        dataToJson(data);
        QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
        (tab == 1) ? ui->teInfo->setText(codec->toUnicode(data)) :
                ui->teInfo_2->setText(codec->toUnicode(data));
        //JsonConvertor *convertor = new JsonConvertor();
        //QString a = convertor->dataByTree(ui->treeWidget);
        //ui->textEdit->setText(a);
    } else {
        transfer_data = false;
        (tab == 1) ? ui->teInfo->setText("Ошибка обмена") :
                ui->teInfo_2->setText("Ошибка обмена");
    }
}

void ListenPort::dataToJson(QByteArray data) {
    JsonConvertor *convertor = new JsonConvertor();
    QString data_string = convertor->dataToJson(data);
    buildJsonTree(data_string);
}

void ListenPort::openPort()
{
    writePortSettings();
    port->setPortName(SettingsPort.name);
    if (port->open(QIODevice::ReadWrite)) {
        if (port->setBaudRate(SettingsPort.baudRate)
                && port->setDataBits(SettingsPort.dataBits)
                && port->setParity(SettingsPort.parity)
                && port->setStopBits(SettingsPort.stopBits)
                && port->setFlowControl(SettingsPort.flowControl)) {
            if (port->isOpen()) {
                ui->teInfo->setText("Open!!!");
                ui->actParams->setEnabled(true);
                ui->actChangeParams->setEnabled(true);
            }
        } else {
            port->close();
            ui->teInfo->setText(port->errorString());
        }
    } else {
        port->close();
        ui->teInfo->setText(port->errorString());
    }
}

void ListenPort::closePort()
{
    if (port->isOpen()) {
        port->close();
    }
    ui->teInfo->setText("Port was closed");
    ui->actParams->setEnabled(false);
    ui->actChangeParams->setEnabled(false);
}

void ListenPort::changePort(QString name)
{
    if (name.isEmpty()) return;
    int ind = port_names.indexOf(name);
    this->setWindowTitle("Address: " + ui->leAddres->text() + "  Name: " + port_discriptions[ind]);
}

void ListenPort::getPortsInfo()
{
    ui->cbPortName->clear();
    port_names.clear();
    port_discriptions.clear();
    port_manufacturers.clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        port_names.push_back(info.portName());
        port_discriptions.push_back(info.description());
        port_manufacturers.push_back(info.manufacturer());
    }
    ui->cbPortName->addItems(port_names);
}
