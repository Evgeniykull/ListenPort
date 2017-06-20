#include "catalogswrither.h"
#include <QString>
#include <QDir>
#include <QFileInfoList>
#include <QFileDialog>
#include <QDebug>
#include <QObject>
#include <vector>

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

    std::vector<QString> name_mass;
    std::vector<QString> type_mass;
    std::vector<QString> len_mass;
    std::vector<QString> addr_mass;
    std::vector<QString> data_mass;
    for (int i = 0; i < files_list.size(); ++i)
    {
        QFileInfo fileInfo = files_list.at(i);
        QString file_name = fileInfo.fileName();

        //type
        QString file_type;
        int dot_pos = file_name.lastIndexOf(".");
        if (dot_pos != -1) {
            file_type = file_name.right(file_name.length() - dot_pos - 1);
            int file_len = file_type.length();
            if (file_len >= 4) {
                file_type = file_type.left(4);
            } else {
                for (int i = file_len; i < 5; i++) {
                    file_type.append("0");
                }
            }
        } else {
            file_type = "0000";
        }
        type_mass.push_back(file_type);

        //name //можно через fileInfo.compliteBaseName()
        file_name = file_name.left(dot_pos);
        int file_name_len = file_name.length();
        if (file_name_len >= 18) {
            file_name = file_name.left(18);
        } else {
            for (int i = file_name_len; i < 19; i++) {
                file_name.append("0");
           }
        }
        file_name.append("0");
        name_mass.push_back(file_name);

        //len
        QByteArray file_size = QByteArray::number(fileInfo.size(), 256);
        QString file_len = file_size;
        if (file_len.length() > 4) {
            file_len = "0000";
        } else {
            for (int i = file_len.length(); i < 4; i++) {
                file_len.insert(0, "0");
            }
        }
        len_mass.push_back(file_len);

        //addr
        QString addr;
        long abs_addr;
        if (i == 0) {
            addr = QString::number(files_list.size() * 32);
        } else {
            abs_addr = addr_mass[i-1].toDouble() + len_mass[i-1].toDouble();
            addr = QString::number(abs_addr);
        }

        if (addr.length() > 4) {
            addr = "0000";
        } else {
            for (int j = addr.length(); j < 4; j++) {
                addr += "0";
            }
        }
        addr_mass.push_back(addr);

        //собираем данные
        QString data_part;
        data_part += addr_mass[i];
        data_part += len_mass[i];
        data_part += type_mass[i];
        data_part += name_mass[i];
        data_part += "0";
        data_mass.push_back(data_part);
    }
    //записать файлы в память

    QFile file("data.cat");
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "Ошибка при открытии файла";
    }
    for (int i = 0; i < data_mass.size(); i++) { //запись информации
        file.write(data_mass[i].toStdString().c_str());
    }
    for (int i = 0; i < files_list.size(); ++i) { //запись данных
        QString path = files_list.at(i).absoluteFilePath();
        QFile *f = new QFile(path);
        if (!f->open(QIODevice::ReadOnly))
        {
            qDebug() << "Ошибка при открытии файла в каталоге";
        }
        QByteArray ba = f->readAll();
        file.write(ba);
    }
    file.close();
}
