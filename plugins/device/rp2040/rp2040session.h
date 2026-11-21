#pragma once
#include <QTimer>

#include "iwp/plotbuffer.h"
#include "iwp/session.h"

// Devices built on the IWP RP2040 base firmware (github.com/eXLuckiEr/IWP-RP2040-base): SCPI over
// USB-CDC. This plugin is a template: it reads what the base firmware offers (identity, chip
// temperature, uptime, heap) and nothing device specific. Copy it for a real instrument and add the
// instrument's commands next to the polls below.
//
// SCPI used: *IDN?  SYSTem:VERSion?  SYSTem:UID?  SYSTem:CLOCk?  SYSTem:UPTime?  SYSTem:HEAP?
//            MEASure:TEMPerature?
//
// Model keys:
//   idn, manufacturer, model, serial, firmware   from *IDN? (read once at start)
//   scpi_version, uid, clock_hz                  read once at start
//   temp_c                                       chip temperature, degrees C (polled)
//   uptime_s                                     seconds since power-up (polled)
//   gpio_map                                     QVariantList of 30 maps {pin, function, output, level, pull}; only while
//                                                the page asks for it (command gpioPolling)
//   gpio_error                                   "" or why GPIO:MAP? failed (old firmware?)
//   heap_free, heap_min                          FreeRTOS heap, bytes: free now, lowest ever (polled)
// Commands: gpioPolling (bool) - read GPIO:MAP? twice a second while true.
// Plot "temperature": one irregular series, degrees C, one point per poll.
class Rp2040Session : public iwp::SessionBase
{
    Q_OBJECT
public:
    Rp2040Session(const Endpoints &endpoints, iwp::DeviceModel *model, QObject *parent = nullptr);

protected:
    void onStart() override;
    void onStopped() override;
    void onCommand(const QString &name, const QVariant &arg) override;

private:
    void readGpio();

    iwp::PlotBuffer *m_plot = nullptr;
    QTimer *m_gpioTimer = nullptr;
    bool m_gpioBusy = false;
};
