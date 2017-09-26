#include "advanced_recording.h"
#include "ui_advanced_recording.h"

advanced_recording::advanced_recording(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::advanced_recording)
{
    ui->setupUi(this);
}

advanced_recording::~advanced_recording()
{
    delete ui;
}