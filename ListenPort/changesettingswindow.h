#ifndef CHANGESETTINGSWINDOW_H
#define CHANGESETTINGSWINDOW_H

#include <QWidget>

namespace Ui {
class ChangeSettingsWindow;
}

class ChangeSettingsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChangeSettingsWindow(QWidget *parent = 0);
    ~ChangeSettingsWindow();
    QString getValue();
    bool isCancel();

signals:
    void pressedOkButton();
    void pressedCancelButton();

private slots:
    void onOkPress();
    void onCancelPress();

private:
    Ui::ChangeSettingsWindow *ui;
    QString value;
    bool cancel = true;
};

#endif // CHANGESETTINGSWINDOW_H
