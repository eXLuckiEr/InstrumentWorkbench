# iwp_add_resources(<target>)
#
# Embeds <module dir>/resources/** (svg, png, jpg, fonts) into the module as Qt resources under the
# prefix :/iwp/<module>/, e.g. plugins/endpoint/ssh/resources/icon.svg -> :/iwp/ssh/icon.svg.
# <module> is the target name without "iwp_". Nothing happens if there is no resources/ directory.
function(iwp_add_resources target)
    set(_dir "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    if(NOT IS_DIRECTORY "${_dir}")
        return()
    endif()
    file(GLOB_RECURSE _files RELATIVE "${_dir}" CONFIGURE_DEPENDS
         "${_dir}/*.svg" "${_dir}/*.png" "${_dir}/*.jpg" "${_dir}/*.ttf" "${_dir}/*.otf")
    if(NOT _files)
        return()
    endif()
    string(REGEX REPLACE "^iwp_" "" _name "${target}")
    set(_abs ${_files})
    list(TRANSFORM _abs PREPEND "${_dir}/")
    qt_add_resources(${target} "${target}_resources"
        PREFIX "/iwp/${_name}" BASE "${_dir}" FILES ${_abs})
endfunction()

# iwp_add_translations(<target>)
#
# Embeds translations of one module (host, sdk or plugin) into the module itself:
# <module dir>/i18n/<target>_<lang>.ts  ->  :/i18n/<target>_<lang>.qm
# Targets "update_translations" (lupdate: collects tr() strings into .ts) and
# "release_translations" (lrelease) are created by Qt. A module without .ts files stays English.
function(iwp_add_translations target)
    if(NOT IWP_TRANSLATIONS)
        return()
    endif()
    file(GLOB _ts CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/i18n/${target}_*.ts")
    if(_ts)
        qt_add_translations(${target} TS_FILES ${_ts} RESOURCE_PREFIX "/i18n")
    endif()
endfunction()

# iwp_add_plugin(<target> SOURCES <files...> [LIBS <libs...>])
#
# Creates a Qt plugin linked against the IWP SDK and places it in
# <output>/plugins, where the host looks for it.
function(iwp_add_plugin target)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    qt_add_plugin(${target} SHARED)
    target_sources(${target} PRIVATE ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE iwp::sdk Qt6::Core Qt6::Widgets ${ARG_LIBS})

    if(APPLE)
        set(_sdk_rpath "@loader_path/..")
    else()
        set(_sdk_rpath "$ORIGIN/..")
    endif()

    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}/plugins"
        BUILD_RPATH   "${_sdk_rpath}"
        INSTALL_RPATH "${_sdk_rpath}")

    iwp_add_translations(${target})
    iwp_add_resources(${target})
endfunction()
