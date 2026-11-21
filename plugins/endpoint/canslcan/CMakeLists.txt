# USB-CAN adapters that speak SLCAN over a virtual serial port (CANable, Lawicel...).
# QSerialPort is an add-on module: skip the plugin gracefully if it is not installed.
if(Qt6SerialPort_DIR MATCHES "NOTFOUND$")
    unset(Qt6SerialPort_DIR CACHE)
endif()
find_package(Qt6 QUIET COMPONENTS SerialPort)
if(NOT Qt6SerialPort_FOUND)
    message(WARNING "Qt6 SerialPort module not found: plugin 'endpoint/canslcan' is skipped.")
    return()
endif()

iwp_add_plugin(iwp_canslcan
    SOURCES
        slcanplugin.h
        slcanplugin.cpp
        slcanendpoint.h
        slcanendpoint.cpp
        slcanconfigeditor.h
        slcanconfigeditor.cpp
        slcancodec.h
    LIBS Qt6::SerialPort)
