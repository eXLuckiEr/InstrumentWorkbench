#pragma once
#include "iwp/session.h"

// Reads sensors and system state of a Linux device with ONE command per
// cycle (cheap on a slow link) and parses "key=value" lines.
// Keys: os, hostname, load1, uptime_s, mem_total_kb, mem_avail_kb,
//       temp/<zone type>/<zone> and hwmon/<chip>/<tempN> (degrees C).
class LinuxHostSession : public iwp::SessionBase
{
    Q_OBJECT
public:
    LinuxHostSession(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;

private:
    void parseLines(const QByteArray &text);
};
