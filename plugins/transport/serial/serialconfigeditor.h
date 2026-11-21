#pragma once
#include "iwp/configeditor.h"

#include <QList>

class QComboBox;
class QGridLayout;
class QPushButton;
class QSpacerItem;
class QWidget;

// Form for the settings of a serial port: port (list of what is connected right now, may be
// typed), speed, frame format, flow control (layout: serialconfigeditor.ui). Keys without a field
// (dtr, rts) stay in the JSON.
class SerialConfigEditor : public iwp::FormConfigEditor
{
    Q_OBJECT
public:
    explicit SerialConfigEditor(QWidget *parent = nullptr);
    QString validate() const override;
    void setConfig(const QVariantMap &config) override;

private:
    void refreshPorts();
    void setAdvanced(bool on);
    QComboBox *m_port = nullptr;
    QPushButton *m_advancedButton = nullptr;
    QGridLayout *m_grid = nullptr;
    QSpacerItem *m_spacer = nullptr;
    QList<QWidget *> m_advancedWidgets;
};
