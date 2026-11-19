#pragma once
#include <QWidget>

#include "iwp/devicemodel.h"

class QDial;
class QDoubleSpinBox;
class QLCDNumber;
class QLabel;
class QPushButton;
class QTimer;

class FakePsuPage : public QWidget
{
    Q_OBJECT
public:
    FakePsuPage(iwp::DeviceModel *model, QWidget *parent = nullptr);

private:
    void refresh(const QString &key);
    void showStatus(iwp::DeviceModel::Status status, const QString &message);

    iwp::DeviceModel *m_model;
    QLabel *m_idn;
    QLabel *m_status;
    QLCDNumber *m_volt;
    QLabel *m_temp;
    QDoubleSpinBox *m_spin;
    QDial *m_dial;
    QPushButton *m_output;
    QTimer *m_sendTimer;
    bool m_updating = false;
};
