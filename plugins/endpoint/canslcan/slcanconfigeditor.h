#pragma once
#include "iwp/configeditor.h"

class QComboBox;
class QPushButton;
class QWidget;

// Settings of an SLCAN adapter: port, bit rate, FD, listen-only... Keys without a field (baud)
// stay in the JSON.
class SlcanConfigEditor : public iwp::FormConfigEditor
{
    Q_OBJECT
public:
    explicit SlcanConfigEditor(QWidget *parent = nullptr);
    QString validate() const override;
    void setConfig(const QVariantMap &config) override;

private:
    void refreshPorts();
    void setAdvanced(bool on);
    QComboBox *m_port = nullptr;
    QPushButton *m_advancedButton = nullptr;
    QWidget *m_advanced = nullptr;
};
