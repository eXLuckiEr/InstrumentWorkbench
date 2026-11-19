#include "telemetryplugin.h"

#include "iwp/icons.h"
#include "telemetrypage.h"
#include "telemetrysession.h"

QList<iwp::EndpointRole> TelemetryPlugin::endpointRoles() const
{
    // Frame layout (little endian), 32 bytes:
    //   0..1 header | 2 counter | 3 id | 4..27 DATA1..6 (float32)
    //   28..29 MUX DATA (u16) | 30..31 CRC16-CCITT-FALSE over bytes 0..29
    QVariantMap framing;
    framing["type"] = "fixed";
    framing["header"] = "AA55";            // TODO: real header bytes, in stream order
    framing["length"] = 32;
    framing["crc"] = "ccitt-false";
    framing["crcOffset"] = 30;
    framing["crcEndian"] = "little";

    iwp::EndpointRole data;
    data.role = QStringLiteral("data");
    data.kind = QStringLiteral("stream");
    data.description = tr("Continuous binary stream from the device");
    data.defaults["framing"] = framing;
    data.defaults["mode"] = "telemetry";   // simulator only, ignored by real transports
    return {data};
}

iwp::SessionBase *TelemetryPlugin::createSession(const QHash<QString, iwp::IEndpoint *> &endpoints,
                                                 iwp::DeviceModel *model)
{
    return new TelemetrySession(endpoints, model);
}

QWidget *TelemetryPlugin::createPage(iwp::DeviceModel *model, QWidget *parent)
{
    return new TelemetryPage(model, parent);
}

QIcon TelemetryPlugin::icon() const
{
    return iwp::themedIcon(QStringLiteral(":/iwp/telemetry/icon.svg"));
}
