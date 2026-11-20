#pragma once
#include "iwp/configeditor.h"

class QComboBox;

// Form for the settings of a serial port: port (list of what is connected right now, may be
// typed), speed, frame format, flow control (layout: serialconfigeditor.ui). Keys without a field
// (dtr, rts) stay in the JSON.
class SerialConfigEditor : public iwp::FormConfigEditor
{
    Q_OBJECT
public:
    explicit SerialConfigEditor(QWidget *parent = nullptr);
    QString validate() const override;

private:
    void refreshPorts();
    QComboBox *m_port = nullptr;
};
