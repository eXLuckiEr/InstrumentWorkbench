#include "cansimplugin.h"

#include "cansimendpoint.h"

QVariantMap CanSimPlugin::defaultConfig() const
{
    QVariantMap cfg;
    cfg["echo"] = true;                       // frames we send come back as received frames
    cfg["rate"] = 50;                         // generated frames per second (0 = none)
    cfg["ids"] = "101,102,103,18FF0001";      // hex IDs of the generator (8 digits = extended)
    cfg["fd"] = false;                        // generate CAN FD frames (16 bytes, BRS)
    return cfg;
}

iwp::IEndpoint *CanSimPlugin::create() { return new CanSimEndpoint; }
