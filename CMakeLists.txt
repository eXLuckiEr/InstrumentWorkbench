cmake_minimum_required(VERSION 3.21)
project(InstrumentWorkbench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# QRhi (the GPU plot renderer) is Qt Gui private API: the application is tied to the Qt build it
# was compiled with, which is fine because Qt is shipped together with it. See docs/plot.md.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg
             OPTIONAL_COMPONENTS LinguistTools ShaderTools GuiPrivate)
qt_standard_project_setup()   # AUTOMOC / AUTOUIC / AUTORCC on

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(IwpPlugin)

# Translations (.ts -> .qm embedded into every module). Needs Qt Linguist Tools (Qt >= 6.7);
# without them the application is simply English only.
if(Qt6LinguistTools_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
    set(IWP_TRANSLATIONS ON)
else()
    set(IWP_TRANSLATIONS OFF)
    message(STATUS "Qt Linguist Tools not found (or Qt < 6.7): translations are not built")
endif()

# Everything (host, sdk, plugins) lands in one runtime tree:
#   bin/<config>/iwp_host, bin/<config>/libiwp_sdk, bin/<config>/plugins/*
set(IWP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

# Product profile = list of plugins that are built and shipped.
set(IWP_PRODUCT "full" CACHE STRING "Product profile (products/<name>.cmake)")
set(_iwp_product_file "${CMAKE_SOURCE_DIR}/products/${IWP_PRODUCT}.cmake")
if(NOT EXISTS "${_iwp_product_file}")
    message(FATAL_ERROR "Unknown product '${IWP_PRODUCT}': ${_iwp_product_file} not found")
endif()
include("${_iwp_product_file}")   # defines IWP_PLUGINS
message(STATUS "IWP product: ${IWP_PRODUCT}; plugins: ${IWP_PLUGINS}")

add_subdirectory(sdk)
add_subdirectory(host)

foreach(_plugin IN LISTS IWP_PLUGINS)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/${_plugin}/CMakeLists.txt")
        message(FATAL_ERROR "Plugin '${_plugin}' listed in product '${IWP_PRODUCT}' does not exist")
    endif()
    add_subdirectory("plugins/${_plugin}")
endforeach()
