#include "simplugin.h"

#include "simtransport.h"

iwp::ITransport *SimPlugin::create() { return new SimTransport; }
