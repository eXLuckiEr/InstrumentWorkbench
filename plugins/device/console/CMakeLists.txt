iwp_add_plugin(iwp_console
    SOURCES
        consoleplugin.h
        consoleplugin.cpp
        consolesession.h
        consolesession.cpp
        consolepage.h
        consolepage.cpp
        commandedit.h
        commandedit.cpp
        commandhistory.h
        commandhistory.cpp
        historycore.h
        hexutil.h
        hexutil.cpp
        hexview.h
        hexview.cpp)

# Terminal mode: xterm.js (plugins/device/console/web, MIT) running in Qt WebEngine, talking to C++
# over QWebChannel. Qt WebEngine is optional: without it the console has the Line and Hex views only.
# Since Qt 6.12 WebEngine is a separate "Extension" (6.140.x) of the Maintenance Tool; add its install
# prefix to CMAKE_PREFIX_PATH if CMake does not find it. See docs/console.md.
option(IWP_CONSOLE_TERMINAL "Console: terminal mode (needs Qt WebEngine)" ON)
if(IWP_CONSOLE_TERMINAL)
    find_package(Qt6 QUIET OPTIONAL_COMPONENTS WebEngineWidgets WebChannel)
    if(TARGET Qt6::WebEngineWidgets AND TARGET Qt6::WebChannel)
        message(STATUS "Console: terminal mode enabled (Qt WebEngine found)")
        target_sources(iwp_console PRIVATE terminalview.h terminalview.cpp)
        target_link_libraries(iwp_console PRIVATE Qt6::WebEngineWidgets Qt6::WebChannel)
        target_compile_definitions(iwp_console PRIVATE IWP_CONSOLE_TERMINAL=1)
        set(_web "${CMAKE_CURRENT_SOURCE_DIR}/web")
        qt_add_resources(iwp_console console_web
            PREFIX "/iwp/console/web" BASE "${_web}"
            FILES ${_web}/terminal.html ${_web}/terminal.js ${_web}/xterm.js
                  ${_web}/xterm.css ${_web}/addon-fit.js)
    else()
        message(STATUS "Console: Qt WebEngine not found, terminal mode disabled (see docs/console.md)")
    endif()
endif()
