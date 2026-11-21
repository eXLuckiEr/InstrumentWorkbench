# CAN simulator: no hardware needed. Sent frames come back (echo) and a generator produces traffic.
iwp_add_plugin(iwp_cansim
    SOURCES
        cansimplugin.h
        cansimplugin.cpp
        cansimendpoint.h
        cansimendpoint.cpp)
