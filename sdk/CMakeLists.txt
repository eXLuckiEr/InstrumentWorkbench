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
    include/iwp/icons.h
    include/iwp/keyvaluepage.h
    include/iwp/session.h
    include/iwp/ifactories.h
    src/itransport.cpp
    src/endpoint.cpp
    src/framers.cpp
    src/streamendpoint.cpp
    src/devicemodel.cpp
    src/icons.cpp
    src/keyvaluepage.cpp
    src/session.cpp
)
add_library(iwp::sdk ALIAS iwp_sdk)

target_include_directories(iwp_sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_definitions(iwp_sdk PRIVATE IWP_SDK_LIBRARY)
target_link_libraries(iwp_sdk PUBLIC Qt6::Core Qt6::Widgets)
target_link_libraries(iwp_sdk PRIVATE Qt6::Svg)

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
