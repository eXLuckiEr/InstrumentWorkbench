find_package(Qt6 REQUIRED COMPONENTS Network)

iwp_add_plugin(iwp_http
    SOURCES
        httpplugin.h
        httpplugin.cpp
        httpendpoint.h
        httpendpoint.cpp
    LIBS Qt6::Network)
