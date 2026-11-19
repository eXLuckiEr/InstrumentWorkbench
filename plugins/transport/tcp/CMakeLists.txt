find_package(Qt6 REQUIRED COMPONENTS Network)

iwp_add_plugin(iwp_tcp
    SOURCES
        tcpplugin.h
        tcpplugin.cpp
        tcptransport.h
        tcptransport.cpp
    LIBS Qt6::Network)
