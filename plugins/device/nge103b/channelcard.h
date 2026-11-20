#pragma once
#include <QGroupBox>
#include <memory>

#include "iwp/devicemodel.h"

class QTimer;
namespace Ui { class ChannelCard; }   // channelcard.ui (Qt Designer)

// One output channel of the NGE: measured V/A, mode, load, setpoint and current limit, protection
// state, enable button. Talks only to the DeviceModel (keys "chN_..." and "master", see
// Nge103bSession). The layout lives in channelcard.ui.
class ChannelCard : public QGroupBox
{
    Q_OBJECT
public:
    ChannelCard(iwp::DeviceModel *model, int channel, QWidget *parent = nullptr);
    ~ChannelCard() override;

private:
    QVariant value(const char *name) const;
    bool isStale(const char *name) const;
    void refresh();
    void updateEnabled();

    iwp::DeviceModel *m_model;
    int m_channel;
    QString m_prefix;                  // "ch1_"
    std::unique_ptr<Ui::ChannelCard> ui;
    QTimer *m_voltTimer;
    QTimer *m_currTimer;
};
