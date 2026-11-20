#pragma once
#include "iwp/plotbuffer.h"
#include "iwp/session.h"

// R&S NGE100B series (NGE103B: 3 channels, 0..32 V, 0..3 A each).
//
// The SCPI of this family works on the *selected* channel (INSTrument OUT1..OUT3), so every request
// carries its own selection: "INST OUT2;MEAS:CURR?". Requests run strictly one after another, so
// polling and user commands cannot mix up each other's channel.
//
// Model keys (N = 1..3):
//   idn, master (bool, OUTP:GEN)
//   chN_v_meas, chN_i_meas       measured V and A
//   chN_mode                     "CV" | "CC" | "OFF"
//   chN_v_set, chN_i_set         setpoint V, current limit A
//   chN_sel                      channel armed (bool, OUTP:SEL)
//   chN_fuse_on, chN_fuse_trip, chN_ovp_trip   (bool)
// Plot "channels" (irregular series, one point per poll): series index (ch-1)*3 + kind, where
// kind is kPlotVolt, kPlotCurr or kPlotPower (power = last voltage x current).
// Commands (argument is a QVariantMap):
//   setVoltage {ch, value}   setCurrent {ch, value}   channelOn {ch, on}   master (bool)
class Nge103bSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    static constexpr int kChannels = 3;
    enum PlotKind { kPlotVolt = 0, kPlotCurr = 1, kPlotPower = 2 };
    static int plotSeries(int channel, PlotKind kind) { return (channel - 1) * 3 + kind; }

    Nge103bSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onStopping(std::function<void()> done) override;
    void onCommand(const QString &name, const QVariant &arg) override;

private:
    enum class Kind { Number, Flag, Text };
    enum class Sample { None, Volt, Curr };   // measured value that also goes to the plot

    // "INST OUTn;<command>"; channel 0 = no selection (master commands).
    static QByteArray onChannel(int channel, const QByteArray &command);
    static QVariant parse(Kind kind, const iwp::Response &response);

    void pollValue(int periodMs, int channel, const QByteArray &command, const QString &key, Kind kind,
                   Sample sample = Sample::None);
    void readValue(int channel, const QByteArray &command, const QString &key, Kind kind, int priority = 0);
    void readChannelState(int channel, int priority = 0);   // setpoints, armed, fuse

    bool m_identified = false;   // *IDN? was answered (worker thread only)
    iwp::PlotBuffer *m_plot = nullptr;
    double m_lastVolt[kChannels] = {};   // worker thread only (poll callbacks)
};
