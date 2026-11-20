find_package(Qt6 REQUIRED COMPONENTS Network)

# Interactive SSH shell for the console. The SSH code (process handling, askpass questions, keychain,
# the settings form) is the one of plugins/endpoint/ssh, compiled into this plugin as well.
set(_ssh "${CMAKE_CURRENT_SOURCE_DIR}/../ssh")

iwp_add_plugin(iwp_sshshell
    SOURCES
        sshshellplugin.h
        sshshellplugin.cpp
        ${_ssh}/sshendpoint.h
        ${_ssh}/sshendpoint.cpp
        ${_ssh}/sshconfigeditor.h
        ${_ssh}/sshconfigeditor.cpp
        ${_ssh}/sshconfigeditor.ui
    LIBS Qt6::Network)
target_include_directories(iwp_sshshell PRIVATE "${_ssh}")

# The same helper as in plugins/endpoint/ssh (defined once when both plugins are built).
if(NOT TARGET iwp_askpass)
    qt_add_executable(iwp_askpass ${_ssh}/askpass/main.cpp)
    target_link_libraries(iwp_askpass PRIVATE Qt6::Core Qt6::Network)
    set_target_properties(iwp_askpass PROPERTIES
        WIN32_EXECUTABLE ON
        RUNTIME_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}")
endif()
