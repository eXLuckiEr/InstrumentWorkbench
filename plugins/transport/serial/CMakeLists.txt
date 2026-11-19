# QSerialPort is an add-on module: skip the plugin gracefully if it is not installed.
# A failed search is cached as "...-NOTFOUND" and would never be retried, even after
# the module gets installed: forget it.
if(Qt6SerialPort_DIR MATCHES "NOTFOUND$")
    unset(Qt6SerialPort_DIR CACHE)
endif()
find_package(Qt6 QUIET COMPONENTS SerialPort)
if(NOT Qt6SerialPort_FOUND)
    message(WARNING "Qt6 SerialPort module not found: plugin 'transport/serial' is skipped. "
                    "Install 'Qt Serial Port' (Additional Libraries) with the Qt Maintenance Tool.")
    return()
endif()

iwp_add_plugin(iwp_serial
    SOURCES
        serialplugin.h
        serialplugin.cpp
        serialtransport.h
        serialtransport.cpp
    LIBS Qt6::SerialPort)
