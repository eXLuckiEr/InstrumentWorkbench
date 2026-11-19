#include "httpplugin.h"

#include "httpendpoint.h"

QVariantMap HttpPlugin::defaultConfig() const
{
    QVariantMap cfg;
    cfg["baseUrl"] = "https://192.168.0.100";   // http:// works too (credentials are NOT protected then)
    cfg["auth"] = "redfish-session";            // none | basic | bearer | redfish-session
    cfg["username"] = "";                       // empty = ask (and remember in the keychain)
    cfg["trustedFingerprint"] = "";             // SHA-256 of the BMC certificate (self-signed)
    cfg["maxConcurrent"] = 4;
    return cfg;
}

iwp::IEndpoint *HttpPlugin::create() { return new HttpEndpoint; }
