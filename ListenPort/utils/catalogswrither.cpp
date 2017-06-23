#include "catalogswrither.h"
#include <QString>
#include <QDir>
#include <QFileInfoList>
#include <QFileDialog>
#include <QDebug>
#include <QObject>
#include <QMessageBox>
#include <vector>
//#include <stdlib.h>
#include <math.h>

catalogswrither::catalogswrither()
{
}

void catalogswrither::writeCatalogToFile() {
    QString dir_path = QFileDialog::getExistingDirectory(0,
            QObject::tr("Select catalog"), "/home");
    if (dir_path.isEmpty()) return;

    QDir dir(dir_path);
    if (!dir.exists()) return;
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList files_list = dir.entryInfoList();

    //std::vector<QString> name_mass;
    std::vector<QByteArray> name_mass;
    std::vector<QByteArray> type_mass;
    std::vector<QByteArray> len_mass;
    std::vector<QByteArray> addr_mass;
    std::vector<QByteArray> data_mass;
    for (int i = 0; i < files_list.size(); ++i)
    {
        QFileInfo fileInfo = files_list.at(i);

        //type
        QByteArray file_type = fileInfo.completeSuffix().toLatin1();
        if (file_type.length() >= 4) {
            file_type = file_type.left(4);
        } else {
            for (int i = file_type.length(); i < 4; i++) {
                file_type.append("0");
            }
        }
        type_mass.push_back(file_type);

        //name
        QByteArray file_name = fileInfo.completeBaseName().toLatin1();
        if (file_name.length() >= 19) {
            file_name = file_name.left(19);
        } else {
            for (int i = file_name.length(); i < 19; i++) {
                file_name.append("0");
           }
        }
        name_mass.push_back(file_name);

        //len
        int fsize = fileInfo.size();
        char ssi[4];
        for (int k = 0; k < 4; k++) { //можно вынести в отдельную функцию
            ssi[k] = (char)(fsize % 256);
            fsize = fsize / 256;
        }

        QByteArray file_len;
        file_len[0] = ssi[3];
        file_len[1] = ssi[2];
        file_len[2] = ssi[1];
        file_len[3] = ssi[0];
        len_mass.push_back(file_len);

        //addr
        unsigned int abs_addr;
        if (i == 0) {
            abs_addr = files_list.size() * 32 + 32;
        } else {
            QByteArray ba = addr_mass[i-1];
            QByteArray ba2 = len_mass[i-1];
            unsigned int _addr, _len;
            unsigned char sym3 = ba[0];
            unsigned char sym2 = ba[1];
            unsigned char sym1 = ba[2];
            unsigned char sym0 = ba[3];

            //проверка на 0
            if (sym3 == 0x00 && sym2 == 0x00 && sym1 == 0x00) {
                _addr = sym0;
            } else if (sym3 == 0x00 && sym2 == 0x00) {
                _addr = sym0 + (sym1 << 8);
            } else if (sym3 == 0x00) {
                _addr = sym0 + (sym1 << 8) + (sym2 << 16);
            } else {
                _addr = (sym3 << 24) + (sym2 << 16) + (sym1 << 8) + sym0;
            }

            sym3 = ba2[0];
            sym2 = ba2[1];
            sym1 = ba2[2];
            sym0 = ba2[3];

            if (sym3 == 0x00 && sym2 == 0x00 && sym1 == 0x00) {
                _len = sym0;
            } else if (sym3 == 0x00 && sym2 == 0x00) {
                _len = sym0 + (sym1 << 8);
            } else if (sym3 == 0x00) {
                _len = sym0 + (sym1 << 8) + (sym2 << 16);
            } else {
                _len = (sym3 << 24) + (sym2 << 16) + (sym1 << 8) + sym0;
            }
            abs_addr = _addr + _len;
        }

        char ssi1[4];
        for (int k = 0; k < 4; k++) {
            ssi1[k] = (char)(abs_addr % 256);
            abs_addr = abs_addr / 256;
        }

        QByteArray addr;
        addr[0] = ssi1[3];
        addr[1] = ssi1[2];
        addr[2] = ssi1[1];
        addr[3] = ssi1[0];
        addr_mass.push_back(addr);

        //собираем данные
        QByteArray data_part;
        data_part += addr_mass[i];
        data_part += len_mass[i];
        data_part += type_mass[i];
        data_part += name_mass[i];
        data_part += "0";
        data_mass.push_back(data_part);
    }

    QFile file("data.bin");
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "Ошибка при открытии файла";
    }

    //запись информации о файлах
    QByteArray info_ba;
    for (int i = 0; i < data_mass.size(); i++) {
        info_ba.append(data_mass[i]);
    }
    file.write(info_ba);

    //запись нулевой строки
    QByteArray null_string;
    for (int i = 0; i < 32; i++) {
        null_string.append("0");
    }
    file.write(null_string);

    //запись данных
    for (int i = 0; i < files_list.size(); ++i) {
        QString path = files_list.at(i).absoluteFilePath();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
        {
            qDebug() << "Ошибка при открытии файла в каталоге";
        }
        QByteArray ba;
        ba = f.readAll();
        file.write(ba);
        f.close();
    }
    file.close();
}

void catalogswrither::getCatalogByFile() {
    QString file_path = QFileDialog::getOpenFileName(
            0,
            QObject::tr("Select file"),
            "/home",
            QObject::tr("File (*.bin)"));
    if (file_path.isEmpty()) {
        qDebug() << "Пустой путь к файлу";
        return;
    }

    QString dir_path = QFileDialog::getExistingDirectory(0,
            QObject::tr("Select catalog"), "/home");
    if (dir_path.isEmpty()) {
        qDebug() << "Пустой путь к каталогу";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(0, QObject::tr("Unable to open file"), file.errorString());
        return;
    }
    QByteArray file_data = file.readAll();

    //поиск нулевой строки
    QString null_string;
    for (int i = 0; i < 32; i++) {
        null_string.append("0");
    }
    int null_pos = file_data.indexOf(null_string);
    while (file_data[null_pos + 32] == '0') { //дальнейший поиск первого символа, который не 0
        null_pos++;
    }
    QByteArray files_info = file_data.left(null_pos);
    int files_num = files_info.length() / 32;
    for (int k = 0; k < files_num; k++) {
        QByteArray fil = files_info.mid(k * 32, 32);
        //разбор файла
        QByteArray f_pos = fil.left(4);
        QByteArray f_len = fil.mid(4, 4);
        QByteArray f_type = fil.mid(8, 4);
        QByteArray f_name = fil.mid(12, 19);

        unsigned char sym3 = f_pos[0];
        unsigned char sym2 = f_pos[1];
        unsigned char sym1 = f_pos[2];
        unsigned char sym0 = f_pos[3];
        unsigned int _f_pos = (sym3 << 24) + (sym2 << 16) + (sym1 << 8) + sym0;
        sym3 = f_len[0];
        sym2 = f_len[1];
        sym1 = f_len[2];
        sym0 = f_len[3];
        unsigned int _f_len = (sym3 << 24) + (sym2 << 16) + (sym1 << 8) + sym0; //ошибки с длинной

        QString full_name = f_name + "." + f_type;
        QFile file(dir_path + "/" + full_name);
        if (!file.open(QIODevice::WriteOnly))
        {
            qDebug() << "Ошибка при создании файла в каталоге";
        }
        QByteArray select_file_data = file_data.mid(_f_pos, _f_len); //не все верно записывается!!!
        file.write(select_file_data);
        file.close();
    }
}
