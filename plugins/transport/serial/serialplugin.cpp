#include "serialplugin.h"

#include <QSerialPortInfo>

#include "serialconfigeditor.h"
#include "serialtransport.h"

QVariantMap SerialPlugin::defaultConfig() const
{
    // Pre-fill the first port that exists right now.
    QString port;
    const auto ports = QSerialPortInfo::availablePorts();
    if (!ports.isEmpty())
        port = ports.first().systemLocation();

    QVariantMap cfg;
    cfg["port"] = port;
    cfg["baud"] = 115200;
    cfg["dataBits"] = 8;
    cfg["parity"] = "none";      // none | even | odd | space | mark
    cfg["stopBits"] = "1";       // 1 | 1.5 (Windows only) | 2
    cfg["flow"] = "none";        // none | hardware (RTS/CTS) | software (XON/XOFF)
    return cfg;
}

iwp::ITransport *SerialPlugin::create() { return new SerialTransport; }

iwp::ConfigEditor *SerialPlugin::createConfigEditor(QWidget *parent)
{
    return new SerialConfigEditor(parent);
}
