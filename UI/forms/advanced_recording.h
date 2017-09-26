#ifndef ADVANCED_RECORDING_H
#define ADVANCED_RECORDING_H

#include <QDialog>

namespace Ui {
class advanced_recording;
}

class advanced_recording : public QDialog
{
    Q_OBJECT

public:
    explicit advanced_recording(QWidget *parent = 0);
    ~advanced_recording();

private:
    Ui::advanced_recording *ui;
};

#endif // ADVANCED_RECORDING_H
