qt_add_executable(iwp_host
    src/main.cpp
    src/pluginmanager.h
    src/pluginmanager.cpp
    src/deviceruntime.h
    src/deviceruntime.cpp
    src/appsettings.h
    src/appsettings.cpp
    src/translations.h
    src/translations.cpp
    src/credentialstore.h
    src/credentialstore.cpp
    src/mainwindow.h
    src/mainwindow.cpp
)
target_link_libraries(iwp_host PRIVATE iwp::sdk Qt6::Core Qt6::Widgets)

# ---- optional: QtKeychain (secrets in the OS keychain) ----
# Build it once against the same Qt and install it, e.g. into ~/Qt/qtkeychain
# (see README), or point QTKEYCHAIN_ROOT at its install prefix.
option(IWP_USE_KEYCHAIN "Store credentials with QtKeychain when it is available" ON)
set(QTKEYCHAIN_ROOT "" CACHE PATH "Install prefix of QtKeychain built for Qt 6")
if(IWP_USE_KEYCHAIN)
    if(Qt6Keychain_DIR MATCHES "NOTFOUND$")
        unset(Qt6Keychain_DIR CACHE)   # a cached failed search would never be retried
    endif()
    find_package(Qt6Keychain CONFIG QUIET
        HINTS "${QTKEYCHAIN_ROOT}" "$ENV{HOME}/Qt/qtkeychain")
    if(Qt6Keychain_FOUND)
        if(TARGET Qt6Keychain::Qt6Keychain)
            set(_iwp_keychain_target Qt6Keychain::Qt6Keychain)
        elseif(TARGET qt6keychain)
            set(_iwp_keychain_target qt6keychain)
        endif()
    endif()
    if(_iwp_keychain_target)
        message(STATUS "QtKeychain found: credentials can be saved (${_iwp_keychain_target})")
        target_link_libraries(iwp_host PRIVATE ${_iwp_keychain_target})
        target_compile_definitions(iwp_host PRIVATE IWP_HAVE_QTKEYCHAIN)
    else()
        message(WARNING "QtKeychain not found: passwords will NOT be saved (asked every time). "
                        "See README, section 'QtKeychain'.")
    endif()
endif()

# application icon of the executable itself (the running window gets it from main.cpp)
if(WIN32)
    target_sources(iwp_host PRIVATE platform/app.rc)
endif()
if(APPLE)
    set(_icns "${CMAKE_CURRENT_SOURCE_DIR}/platform/app.icns")
    target_sources(iwp_host PRIVATE "${_icns}")
    set_source_files_properties("${_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(iwp_host PROPERTIES MACOSX_BUNDLE_ICON_FILE app.icns)
endif()

if(APPLE)
    set(_origin "@loader_path")
else()
    set(_origin "$ORIGIN")
endif()
set_target_properties(iwp_host PROPERTIES
    WIN32_EXECUTABLE ON
    RUNTIME_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}"
    BUILD_RPATH "${_origin}")

iwp_add_translations(iwp_host)
iwp_add_resources(iwp_host)
