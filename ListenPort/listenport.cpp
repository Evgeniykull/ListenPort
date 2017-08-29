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
#include <QInputDialog>


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
    connect(ui->pbUpdateInfo, SIGNAL(clicked(bool)), this, SLOT(updateInfo()));
    connect(ui->pbUpdateSettings, SIGNAL(clicked(bool)), this, SLOT(updateSettings()));

    connect(ui->cbPortName, SIGNAL(currentTextChanged(QString)), this, SLOT(changePort(QString)));
    connect(ui->pbSavePortSettings, SIGNAL(clicked()), this, SLOT(writePortSettings())); //поменять вызов

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
    connect(ui->actGetCatalog, SIGNAL(triggered()), this, SLOT(getCatalog()));
    connect(ui->actWriteFileToDevice, SIGNAL(triggered()), this, SLOT(writeFileToDevice()));
    connect(ui->actReadFileFromDevice, SIGNAL(triggered()), this, SLOT(readFileFromDevice()));

    connect(ui->actExchange, SIGNAL(triggered(bool)), this, SLOT(changeViewMod()));

    getPortsInfo();
    getPortSettingsFromFile();
}

ListenPort::~ListenPort()
{
    delete ui;
}

void ListenPort::changeViewMod() {
    switch (tab) {
    case 1:
        tab = 3;
        ui->tabWidget->setCurrentIndex(3);
        break;
    case 2:
        tab = 4;
        ui->tabWidget->setCurrentIndex(4);
        break;
    case 3:
        tab = 1;
        ui->tabWidget->setCurrentIndex(1);
        break;
    case 4:
        tab = 2;
        ui->tabWidget->setCurrentIndex(2);
        break;
    default:
        break;
    }
}

void ListenPort::writeCatalog() {
    catalogswrither catalog;
    QByteArray catalog_data = catalog.prepearFile();
    catalog.writeCatalogToFile(catalog_data);
}

void ListenPort::getCatalog() {
    catalogswrither catalog;
    catalog.getCatalogByFile();
}

void ListenPort::writeFileToDevice() {
    openPort();
    if (!port->isOpen()) {
        QMessageBox::information(0, QObject::tr("Can not write to device"), QObject::tr("Port closed"));
        return;
    }

    QString file_path = QFileDialog::getOpenFileName(
            0,
            QObject::tr("Select file"),
            "/home",
            QObject::tr("File (*.bin)"));
    if (file_path.isEmpty()) {
        QMessageBox::information(0, QObject::tr("Ошибка при получении каталога"), "Пустой путь к файлу");
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(0, QObject::tr("Невозможно открыть файл"), file.errorString());
        return;
    }

    QByteArray catalog_data = file.readAll();
    int catalog_data_len = catalog_data.length();
    int count = 0;
    QByteArray lbl_settings;

    setToolTip("Идет обмен");
    setStatusTip("Идет обмен");
    QMessageBox::information(0, QObject::tr("Update Settings"), QObject::tr("Пожалуйста подождите"));
    delay(10);
    while (catalog_data_len / 256 >= 0) {
        int len = (catalog_data_len - count*256) >= 256 ? 256 : len; // нужно ли?
        if (len <= 0) return;
        QByteArray mid_data = catalog_data.mid(count*256, len);
        lbl_settings = "set Memory1[" + QByteArray::number(count) + "]:{" +
                        "Data:\"" + mid_data.toHex().toUpper() + '\0' + "\"}";
        count++;
        QByteArray write_data = writeData(lbl_settings);
        if(!write_data.length()) {
            break;
        }
        catalog_data_len = catalog_data_len / 256;
    }
    setToolTip("");
    closePort();
}

void ListenPort::readFileFromDevice() {
    openPort();
    if (!port->isOpen()) {
        QMessageBox::information(0, QObject::tr("Can not write to device"), QObject::tr("Port closed"));
        return;
    }
    QByteArray answ_get = writeData("get Memory1");
    int pos = answ_get.indexOf("length:");
    answ_get = answ_get.mid(pos + 7);
    pos = answ_get.indexOf('\r');
    answ_get = answ_get.mid(0, pos);

    QDataStream ds(answ_get);
    int len;
    ds >> len;

    bool ok;
    QString user_len_for_read = QInputDialog::getText(0, "Read file fom device", "Input file length:",
                                              QLineEdit::Normal, "", &ok);
    if (!ok) return; //вывести ошибку
    int user_len = user_len_for_read.toInt(&ok);
    if (!ok) return; //вывести ошибку

    QString file_name = QInputDialog::getText(0, "Read file fom device", "Input bin file name:",
                                              QLineEdit::Normal, "", &ok);
    if (!ok) return;

    QFile file(file_name + ".bin");
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(0, QObject::tr("Read file fom device"), "Ошибка при создании файла");
    }

    if (user_len > len) return;

    QByteArray mid_data = " ";
    int iteration = 0;
    while(mid_data != "" && iteration < user_len ) {
        QByteArray query = "get Memory1[" + QByteArray::number(iteration) + "]";
        mid_data = writeData(query);
        int start_pos = mid_data.indexOf("{") + 9;
        int end_pos = mid_data.indexOf("}") - start_pos - 3;
        mid_data = mid_data.mid(start_pos, end_pos);
        file.write(mid_data);
        iteration++;
    }
    file.close();
    closePort();
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
    QString data_string = dataToJson(file_data);
    buildJsonTree(data_string);
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
    msgBox.setText("Cannot load json to tree."); //to form
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
        msgBox.setText("Cannot convert json to tree."); //to form
        msgBox.exec();
    }
}

void ListenPort::onParamsClick() {
    last_index = 2;
    ui->tabWidget->setCurrentIndex(2);
    tab = 2;
    if(ui->treeWidget_2->topLevelItemCount() <= 0) { //если пусто - обновить
        updateInfo();
    }
}

void ListenPort::onChangeParamsClick() {
    last_index = 1;
    ui->tabWidget->setCurrentIndex(1);
    tab = 1;
    if(ui->treeWidget_2->topLevelItemCount() <= 0) { //если пусто - обновить
        updateSettings();
    }
}

void ListenPort::updateInfo() {
    openPort();
    if (!port->isOpen()) {
        QMessageBox::information(0, QObject::tr("Can not write to device"), QObject::tr("Port closed"));
        return;
    }
    setToolTip("Идет обмен");
    setStatusTip("Идет обмен");
    QMessageBox::information(0, QObject::tr("Update Info"), QObject::tr("Please wait"));
    QByteArray get_data = writeData("get Info");
    if (get_data.length()) {
        setStatusTip("Обмен завершен успешно");
        ui->txtBrwsInfo->setText(get_data);
        QString data_string = dataToJson(get_data);
        buildJsonTree(data_string);
    }
    setToolTip("");
    closePort();
}

void ListenPort::updateSettings() {
    openPort();
    if (!port->isOpen()) {
        QMessageBox::information(0, QObject::tr("Can not write to device"), QObject::tr("Port closed"));
        return;
    }
    setToolTip("Идет обмен");
    setStatusTip("Идет обмен");
    QMessageBox::information(0, QObject::tr("Update Settings"), QObject::tr("Please wait"));
    QByteArray get_data = writeData("get Settings");
    if (get_data.length()) {
        QByteArray get_data2 = analizeData(get_data);
        setStatusTip("Обмен завершен успешно");
        ui->txtBrwsSetting->setText(get_data2);
        QString data_string = dataToJson(get_data2);
        QFile fl("pw1.txt");
        fl.open(QIODevice::WriteOnly);
        fl.write(data_string.toUtf8());
        fl.close();
        buildJsonTree(data_string);
    }
    setToolTip("");
    closePort();
}

void ListenPort::onSettingsClick(bool enab) {
    if (enab) {
        getPortsInfo();
        ui->tabWidget->setCurrentIndex(0);
        return;
    }
    ui->tabWidget->setCurrentIndex(last_index);
}

void ListenPort::getPortSettingsFromFile() { //переделать на словарь
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
        if (list[0] == "ByteInPacket") {
            ui->leByteInPacket->setText(list[1]);
            continue;
        }
    }
    file.close();

    SettingsPort.name = ui->cbPortName->itemText(ui->cbPortName->currentIndex());
    SettingsPort.baudRate = (QSerialPort::BaudRate) ui->cbBaudRate->itemText(ui->cbBaudRate->currentIndex()).toInt();
    SettingsPort.dataBits = (QSerialPort::DataBits) ui->cbDataBist->itemText(ui->cbDataBist->currentIndex()).toInt();
    SettingsPort.parity = (QSerialPort::Parity) ui->cbPairity->itemText(ui->cbPairity->currentIndex()).toInt();
    SettingsPort.stopBits = (QSerialPort::StopBits) ui->cbStopBits->itemText(ui->cbStopBits->currentIndex()).toInt();
    SettingsPort.flowControl = (QSerialPort::FlowControl) ui->cbFlowControl->itemText(ui->cbFlowControl->currentIndex()).toInt();
}

void ListenPort::setSavingParams() {
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

    bool ok;
    int onPackage = ui->leAddres->text().toInt(&ok);
    byteOnPackage = (ok && onPackage < 256 && onPackage > 0) ? onPackage : 100;

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
    out << "ByteInPacket=" << ui->leByteInPacket->text() << "\n";
    file.close();
}

QByteArray ListenPort::analizeData(QByteArray data) {
    QByteArray substr;
    int gtt_len = 0;
    int pos = data.indexOf("]");
    while(pos > 0) {
        if (data[pos-1] == '[') {
            int posa = data.indexOf("{", pos+1);
            int posb = data.indexOf("[", pos+1);
            if ((posb < posa || posa == -1) && posb > 0) {
                pos = data.indexOf("]", pos+1+gtt_len);
                continue;
            }
            substr = data.mid(0, pos);
            int pos_start = substr.lastIndexOf('\n');
            substr = substr.mid(pos_start + 1, pos - pos_start - 2);
            if (substr == "") {
                pos = data.indexOf("]", pos+1+gtt_len);
                continue;
            }

            QByteArray new_data = writeData("get Settings." + substr);
            int fst_kv = new_data.indexOf("{");
            int last_kv = new_data.lastIndexOf("}");
            new_data = new_data.mid(fst_kv, last_kv - fst_kv + 1); //json object

            QString st = dataToJson(new_data);
            QJsonDocument json = QJsonDocument::fromJson(st.toUtf8());
            if (json.isNull()) {
                pos = data.indexOf("]", pos+1+gtt_len);
                continue;
            }
            if (!json.isObject()) {
                pos = data.indexOf("]", pos+1+gtt_len);
                continue;
            }
            QJsonObject obj = json.object();
            int st1 = obj["start"].toInt();
            int ln = obj["length"].toInt();

            QByteArray getted_data;
            for (int i = st1; i < ln; i++) {
                QByteArray part = writeData("get Settings." + substr + "[" + QByteArray::number(i) + "]");
                //обрезать answ...
                int dv_pos = part.indexOf(":") + 1;
                int end_pos = part.lastIndexOf("}");
                part = part.mid(dv_pos, end_pos - dv_pos+1);
                getted_data.append("[" + QByteArray::number(i) + "]:");
                getted_data.append(part);
                getted_data.append("\n");
            }
            getted_data = getted_data.mid(0, getted_data.length());
            QByteArray getted_data_2;
            getted_data_2.append("{");
            getted_data_2.append(getted_data);
            getted_data_2.append("}");
            /*QString data_string = dataToJson(getted_data_2);
            QFile fl("pw.txt");
            fl.open(QIODevice::WriteOnly);
            fl.write(data_string.toUtf8());
            fl.close();*/


            //правильно добавить
            int pppos = data.indexOf("}", pos+1);
            QByteArray sss = data.mid(pppos+1);

            QByteArray firs = data.mid(0, pos+1);
            //QByteArray sec = data.mid(pos+1);
            data = firs + ":" + getted_data_2 + sss;
            gtt_len = getted_data_2.length();
        }
        pos = data.indexOf("]", pos+1+gtt_len);
        gtt_len = 0;
    }
    return data;
}

QByteArray ListenPort::writeData(QByteArray text)
{
    if (transfer_data) {
        return "";
    } else {
        transfer_data = true;
    }

    //Подготавливаем данные
    QByteArray data = text;
    int data_len = data.length();
    if (data_len == 0) {
        return "";
    }

    int iter;
    unsigned char addres = ui->leAddres->text().toInt();
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
            transfer_data = false;
            return "";
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
        return data;
        //QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
        //codec->toUnicode(data);  //то, что пришло в формате читаемой строки
    }

    transfer_data = false;
    QMessageBox::information(0, QObject::tr("Write to device"),
            QObject::tr("Ошибка обмена"));
    return "";
}

QString ListenPort::dataToJson(QByteArray data) {
    JsonConvertor *convertor = new JsonConvertor();
    return convertor->dataToJson(data);
}

void ListenPort::openPort()
{
    writePortSettings();
    port->setPortName(SettingsPort.name);
    if(port->isOpen()) {
        return;
    }
    if (port->open(QIODevice::ReadWrite)) {
        if (port->setBaudRate(SettingsPort.baudRate)
                && port->setDataBits(SettingsPort.dataBits)
                && port->setParity(SettingsPort.parity)
                && port->setStopBits(SettingsPort.stopBits)
                && port->setFlowControl(SettingsPort.flowControl)) {
            if (port->isOpen()) {
                setStatusTip("Port is open");
            }
        } else {
            port->close();
            setStatusTip(port->errorString());
        }
    } else {
        port->close();
        setStatusTip(port->errorString());
    }
}

void ListenPort::closePort()
{
    if (port->isOpen()) {
        port->close();
    }
    setStatusTip("Port is closed");
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
