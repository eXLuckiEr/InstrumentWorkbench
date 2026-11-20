find_package(Qt6 REQUIRED COMPONENTS Network)

iwp_add_plugin(iwp_ssh
    SOURCES
        sshplugin.h
        sshplugin.cpp
        sshendpoint.h
        sshendpoint.cpp
        sshconfigeditor.h
        sshconfigeditor.cpp
    LIBS Qt6::Network)

# Helper started by OpenSSH (SSH_ASKPASS) when it needs a password, a PIN or a
# confirmation. It forwards the question to the endpoint, which asks the user
# through the host. Must sit next to the application executable.
qt_add_executable(iwp_askpass askpass/main.cpp)
target_link_libraries(iwp_askpass PRIVATE Qt6::Core Qt6::Network)
set_target_properties(iwp_askpass PROPERTIES
    WIN32_EXECUTABLE ON      # no console window flashing up on Windows
    RUNTIME_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}")
