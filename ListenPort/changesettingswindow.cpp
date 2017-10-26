#include "changesettingswindow.h"
#include "ui_changesettingswindow.h"

ChangeSettingsWindow::ChangeSettingsWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChangeSettingsWindow)
{
    ui->setupUi(this);
    connect(ui->pb_Ok, SIGNAL(clicked()), this, SLOT(onOkPress()));
    connect(ui->pb_Cancel, SIGNAL(clicked()), this, SLOT(onCancelPress()));
}

ChangeSettingsWindow::~ChangeSettingsWindow()
{
    delete ui;
}

void ChangeSettingsWindow::onOkPress() {
    value = ui->le_Value->text();
    cancel = false;
    pressedOkButton();
}

void ChangeSettingsWindow::onCancelPress() {
    cancel = true;
    pressedCancelButton();
}

QString ChangeSettingsWindow::getValue() {
    return value;
}

bool ChangeSettingsWindow::isCancel() {
    return cancel;
}
