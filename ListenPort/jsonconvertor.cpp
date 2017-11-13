#include "jsonconvertor.h"
#include <QString>
#include <QTextCodec>
#include <vector>
#include <QDebug>

using namespace std;

JsonConvertor::JsonConvertor()
{
}

QString JsonConvertor::jsonToData(QByteArray json) {
    return "";
}

//удалять сразу будет дольше
//сначало поиск, потом создание новой строки
QString addSpaseAfterChar(QString str, const char* ch) {
    int from = 0;
    vector<int> positions;
    while (int pos = str.indexOf(ch, from)) {
        if (pos == -1) {
            break;
        }
        from = pos + 1;
        positions.push_back(from);
    }
    if (positions.size() > 0) { //вынести в отдельную функцию
        QString new_data;
        int pos = 0;
        for (int i = 0; i < positions.size(); i++) {
            new_data += str.mid(pos, positions[i] - pos) + " ";
            pos = positions[i];
        }
        str = new_data + str.mid(pos, str.length() - pos);
    }
    return str;
}

QString JsonConvertor::dataToJson(QByteArray data) {
    QString data_string = data;

    //обрезка первых 2х значений (answ get)
    int pos = data_string.indexOf("{");
    data_string = data_string.right(data_string.length() - pos - 1);
    pos = data_string.lastIndexOf("}");
    data_string = data_string.left(pos) + "\0";

    //если 2я с начала \n, удаляем 2 символа, иначе 1
    pos = data_string.indexOf('\n');
    if (pos == 1) {
        data_string.remove(0, 2);
    } else {
        data_string.remove(0, 1);
    }


    //Добавление пробела после двоеточия
    data_string = addSpaseAfterChar(data_string, ":");

    //Добавление пробела после запятой
    data_string = addSpaseAfterChar(data_string, ",");

    //Добавление пробела после {
    data_string = addSpaseAfterChar(data_string, "{");

    //Добавление ковычек
    int all_kow = 0;
    for (int i=0; i< data_string.length(); i++) {
        QChar chart = data_string[i];
        if (chart == '"') {
            all_kow++;
            continue;
        }
        if (chart == ':') {
            if (all_kow % 2 != 0) continue;

            int space_pos = data_string.lastIndexOf(" ", i);
            int enter_pos = data_string.lastIndexOf("\n", i);
            int space = (space_pos > enter_pos) ? space_pos : enter_pos;
            data_string.insert(space + 1, "\"");
            data_string.insert(i + 1, "\"");
            //all_kow++;
            i += 3;
        }
    }

    //добавление запятых (неоптимально)
    int from = 0;
    while (int start_pos = data_string.indexOf("\n", from)) {
        if (start_pos == -1 || start_pos >= data_string.length() - 2) {
            break;
        }
        int symb_pos1 = data_string.lastIndexOf(" ", start_pos);
        int symb_pos2 = data_string.lastIndexOf("{", start_pos);
        if (symb_pos2 == 0 || symb_pos2 == start_pos - 2 ||
                symb_pos2 == symb_pos1 - 2) {
            from = start_pos + 1;
            continue;
        }

        symb_pos1 = data_string.lastIndexOf("}", start_pos);
        symb_pos2 = data_string.indexOf("\"", start_pos);
        int symb_pos3 = data_string.indexOf("}", start_pos);
        if (symb_pos2 == start_pos - 2 || symb_pos1 == start_pos + 2 ||
                symb_pos3 == start_pos + 2 || symb_pos3 == start_pos + 1) {
            from = start_pos + 1;
            continue;
        }

        data_string.insert(start_pos, ",");
        from = start_pos + 2;
    }

    //Добавление объемлющих {}
    data_string = "{\n" + data_string;
    data_string.insert(data_string.length() - 1, "}");

    //удаление \r
    while (int n = data_string.indexOf("\r")) {
        if (n == -1) {
            break;
        }
        data_string.remove(n, 1);
    }

    //удаление , после {
    int n;
    while (n = data_string.indexOf("{ ,")) {
        if (n == -1) {
            break;
        }
        data_string.remove(n + 2, 1);
    }

    qDebug() << data_string;
    return data_string;
}

void tree(QTreeWidgetItem * item, QString & data, bool is_mass = false) {
    if (!is_mass) {
        data += item->text(0);
        if (item->childCount() == 0) {
            bool ok = 0;
            int n = item->text(1).toDouble(&ok);
            data += ok ? ":" + item->text(1) + "\n" : ":\"" + item->text(1) + "\"\n";
            return;
        }
    }

    if (is_mass) {
        if (item->childCount() == 0) {
            //проверка на число ли это
            bool ok;
            double n = item->text(1).toDouble(&ok);
            (ok) ? data += QString::number(n) + "," :
                data += "\"" + item->text(1) + "\",";
            return;
        }
    }

    if (item->text(1).isEmpty()) {
        if (data.lastIndexOf("]") == data.length() - 1) {
            data += ":[";
            for (int i = 0; i < item->childCount(); i++) {
                tree(item->child(i), data, true);
            }
            data.remove(-1, 1);
            data += "]\n";
        } else {
            data += ":{\n";
            for (int i = 0; i < item->childCount(); i++) {
                tree(item->child(i), data);
            }
            data += "}\n";
        }
    }

    while (int pos =  data.indexOf("\n:{")) {
        if (pos == -1) break;
        data.remove(pos, 2);
        data.insert(pos, ",");
    }

    while (int pos =  data.indexOf("[:{")) {
        if (pos == -1) break;
        data.remove(pos + 1, 1);
    }
}

QString JsonConvertor::dataByTree(QTreeWidget *tree_stuct) {
    QString data;
    for (int i = 0; i < tree_stuct->topLevelItemCount(); i++) {
        tree(tree_stuct->topLevelItem(i), data);
    }
    return data;
}


