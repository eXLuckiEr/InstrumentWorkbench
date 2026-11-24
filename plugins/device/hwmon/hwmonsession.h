#pragma once
#include <QByteArray>
#include <QTimer>
#include <QVector>

#include "hwmoncore.h"
#include "iwp/plotbuffer.h"
#include "iwp/session.h"

// Sensors of a Linux device (for example an OpenBMC) read over one SSH connection, see hwmoncore.h for
// the way it works. The role "stream" is the SSH shell endpoint; its default remote command (set by the
// plugin) starts the receiver, the session sends the script.
//
// Model keys:
//   info             QVariantList of {name, text}: output of the info commands, in the order of the settings
//   layout           QVariantList of {name, unit, decimals, text, hasMin, minV, hasMax, maxV, expect}: the slots the
//                    device really reads (a mask gives several); empty until the device reports them
//   values           QVariantList, one per slot: double (already divided) or QString for a text sensor
//   sensor_count, period_ms   what the device runs now
// Plot "sensors": one regular series per slot (kMaxSensors), values divided by the divisor (text slots: no data).
// Events: "log" (text) for messages from the device.
// Commands: reload - read the settings again and restart the script on the device.
class HwmonSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    HwmonSession(const Endpoints &endpoints, iwp::DeviceModel *deviceModel, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onStopped() override;
    void onCommand(const QString &name, const QVariant &arg) override;

private:
    void loadConfig();
    void sendScript();
    void onData(const QByteArray &data);
    void onLine(const std::string &line);
    void flush();
    void resetLayout();
    void applySlots();

    iwp::PlotBuffer *m_plot = nullptr;
    hwmon::Config m_cfg;
    QString m_cfgError;
    hwmon::LineBuffer m_lines;
    QTimer *m_flushTimer;
    QTimer *m_helloTimer;
    QTimer *m_readyTimer;
    bool m_scriptSent = false;
    bool m_ready = false;
    QVariantList m_info;
    QVector<QVector<float>> m_batch;
    QVector<float> m_last;
    QVector<double> m_divisor;                          // per slot
    std::vector<std::vector<std::string>> m_pendingL;   // L messages before R
    std::vector<hwmon::Slot> m_slots;
    QStringList m_texts;
    bool m_valuesDirty = false;
};
