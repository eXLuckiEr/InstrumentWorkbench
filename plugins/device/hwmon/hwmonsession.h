#pragma once
#include <QElapsedTimer>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <vector>

#include "hwmoncore.h"
#include "iwp/plotbuffer.h"
#include "iwp/session.h"

// Sensors of a Linux device (for example an OpenBMC) read over the ordinary SSH command channel, see
// hwmoncore.h for the way it works: a discovery script after connecting, then a poll script every period.
//
// Model keys:
//   info             QVariantList of {name, text}: output of the info commands, in the order of the settings
//   layout           QVariantList of {name, unit, decimals, text, hasMin, minV, hasMax, maxV, expect}: the slots the
//                    device really reads (a mask gives several); empty until the device reports them
//   values           QVariantList, one per slot: double (already divided) or QString for a text sensor
//   diag             QString: lines of an answer that are not part of the protocol (messages of the remote
//                    shell); empty when there were none
//   sensor_count, period_ms   what is polled now
// Plot "sensors": one irregular series per slot (kMaxSensors), values divided by the divisor (text slots: no data).
// Events: "log" (text) for problems the device reported (a sensor file is missing...).
// Commands: reload - read the settings again and discover the device again (no reconnect).
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
    bool loadConfig();                 // false: the settings are wrong (m_cfgError says what)
    void discover(bool firstStart);
    void pollOnce();
    void onSample(const QByteArray &answer);
    void resetLayout();
    void publishValues();
    void noteStray(const std::vector<std::string> &lines);

    iwp::PlotBuffer *m_plot = nullptr;
    hwmon::Config m_cfg;
    QString m_cfgError;
    QTimer *m_pollTimer;
    QElapsedTimer m_valuesClock;       // the page is told about the values at most every 30 ms
    bool m_busy = false;               // a poll is on its way: the next tick is skipped (a slow link)
    int m_generation = 0;              // changes on reload / stop: answers of the old one are ignored
    QByteArray m_pollScript;
    std::vector<hwmon::Slot> m_slots;
    QVector<float> m_last;
    QStringList m_texts;
    QVector<double> m_divisor;         // per slot
    QStringList m_noise;
};
