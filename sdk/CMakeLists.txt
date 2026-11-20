add_library(iwp_sdk SHARED
    include/iwp/global.h
    include/iwp/version.h
    include/iwp/itransport.h
    include/iwp/credentials.h
    include/iwp/endpoint.h
    include/iwp/iframer.h
    include/iwp/framers.h
    include/iwp/bytes.h
    include/iwp/streamendpoint.h
    include/iwp/devicemodel.h
    include/iwp/configeditor.h
    include/iwp/icons.h
    include/iwp/keyvaluepage.h
    include/iwp/session.h
    include/iwp/plotbuffer.h
    include/iwp/plotview.h
    include/iwp/ifactories.h
    src/itransport.cpp
    src/endpoint.cpp
    src/framers.cpp
    src/streamendpoint.cpp
    src/devicemodel.cpp
    src/configeditor.cpp
    src/icons.cpp
    src/keyvaluepage.cpp
    src/session.cpp
    src/plotcore.h
    src/plotframe.h
    src/plotbuffer.cpp
    src/plotview.cpp
)
add_library(iwp::sdk ALIAS iwp_sdk)

target_include_directories(iwp_sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_definitions(iwp_sdk PRIVATE IWP_SDK_LIBRARY)
target_link_libraries(iwp_sdk PUBLIC Qt6::Core Qt6::Widgets)
target_link_libraries(iwp_sdk PRIVATE Qt6::Svg)

# GPU renderer of PlotView: QRhiWidget (Qt >= 6.7) and shaders compiled by Qt Shader Tools.
# Without them PlotView simply uses its QPainter renderer.
if(Qt6ShaderTools_FOUND AND Qt6GuiPrivate_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
    target_sources(iwp_sdk PRIVATE src/plotgpu.h src/plotgpu.cpp)
    target_link_libraries(iwp_sdk PRIVATE Qt6::GuiPrivate)   # QRhi lives in Qt Gui private API
    target_compile_definitions(iwp_sdk PRIVATE IWP_PLOT_RHI)
    qt_add_shaders(iwp_sdk "iwp_sdk_plot_shaders"
        PREFIX "/iwp/plot"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        FILES shaders/plot.vert shaders/plot.frag)
    message(STATUS "PlotView: GPU renderer enabled (QRhiWidget)")
else()
    message(STATUS "PlotView: Qt Shader Tools / Gui private headers not found (or Qt < 6.7): CPU renderer only")
endif()

if(APPLE)
    set(_origin "@loader_path")
else()
    set(_origin "$ORIGIN")
endif()
set_target_properties(iwp_sdk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${IWP_OUTPUT_DIR}"
    BUILD_RPATH "${_origin}")

iwp_add_translations(iwp_sdk)
iwp_add_resources(iwp_sdk)
