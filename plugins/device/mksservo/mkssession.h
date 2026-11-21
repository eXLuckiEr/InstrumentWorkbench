#pragma once
#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QSet>
#include <QVariant>
#include <functional>
#include <vector>

#include "iwp/canendpoint.h"
#include "iwp/session.h"
#include "mkscore.h"

class QTimer;

// Several MKS SERVO42D/57D motors on one CAN bus. The session does the request/answer exchange
// (one request in flight, matched by motor ID and command code, with a timeout), polls the motors
// and publishes their state. Commands from the page:
//   "setIds"      QVariantList of int           motors to poll (the "Motors" list)
//   "select"      int                           the motor shown in detail (polled faster)
//   "scan"        {from, to}                    probe addresses with the version command
//   "request"     {id, code, params(QByteArray), reply(int, optional), timeout(ms, optional), note(text)}
//   "readConfig"  {id}                          read endstop / home / mode parameters
//   "estopAll"                                  emergency stop to all motors (broadcast)
//   "verbose"     bool                          log every poll request too
// Model: key "motors" = QVariantList of maps (id, online, ...), "connection";
// events: "result" {id, code, ok, status, text}, "config" {id, code, ok, params}, "log" text,
// "scan" {done, found}.
class MksSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    MksSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onStopped() override;
    void onCommand(const QString &name, const QVariant &arg) override;

private:
    struct Motor {
        int id = 0;
        bool online = false;
        int fails = 0;
        bool haveVersion = false;
        mks::VersionInfo version;
        bool havePos = false;
        qint64 posCounts = 0;
        int speed = 0;
        int runStatus = -1;
        bool haveIo = false;
        mks::IoState io;
        int stall = -1;
        int enabled = -1;
        mks::VoltCurrent vi;
        bool haveVi = false;
        double angleError = 0;
        int home1 = -1, home2 = -1;
        QString note;   // last unsolicited event ("homing done", "stopped by limit"...)
    };
    struct Pending {
        int id = 0;
        quint8 code = 0;
        std::vector<quint8> params;
        int replyCode = -1;        // code of the answer, default = code (for "00 <code>" the read code)
        int timeoutMs = 100;
        int priority = 0;
        bool expectReply = true;
        bool logIt = false;        // a user action: show it in the log
        std::function<void(bool, const iwp::can::Frame *)> done;
    };

    Motor &motor(int id);
    void enqueue(Pending p);
    void pump();
    void sendCurrent();
    void onFrame(const iwp::can::Frame &frame);
    void onTimeout();
    void finishCurrent(bool ok, const iwp::can::Frame *frame);
    void applyReply(Motor &m, int cmd, const quint8 *p, int n);
    void pollTick();
    void publish();
    void log(const QString &text);
    void scan(int from, int to);
    void readConfig(int id);
    static QString describe(int cmd, const quint8 *p, int n);

    iwp::CanEndpoint *m_bus = nullptr;
    QTimer *m_pollTimer;
    QTimer *m_publishTimer;
    QTimer *m_replyTimer;
    QMap<int, Motor> m_motors;
    QList<int> m_ids;
    int m_selected = -1;
    QList<Pending> m_queue;
    bool m_busy = false;
    Pending m_cur;
    int m_tick = 0;
    bool m_dirty = false;
    bool m_verbose = false;
    QElapsedTimer m_clock;
};
