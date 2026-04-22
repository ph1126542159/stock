# Sources
file(GLOB SRCS_G "src/*.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

add_library(OpenTelemetry ${SRCS})
add_library(Poco::OpenTelemetry ALIAS OpenTelemetry)
set_target_properties(OpenTelemetry
    PROPERTIES
    VERSION ${SHARED_LIBRARY_VERSION} SOVERSION ${SHARED_LIBRARY_VERSION}
    OUTPUT_NAME PocoOpenTelemetry
    DEBUG_POSTFIX d
    DEFINE_SYMBOL OpenTelemetry_EXPORTS
)

target_link_libraries(OpenTelemetry
    PUBLIC
        Poco::OSP
        Poco::JSON
        Poco::Net
        Poco::Util
        Poco::Foundation
        myiot_opentelemetry
)

if(TARGET myiot_opentelemetry_otlp_http)
    target_link_libraries(OpenTelemetry PRIVATE myiot_opentelemetry_otlp_http)
    target_compile_definitions(OpenTelemetry PRIVATE MYIOT_OPENTELEMETRY_HAS_OTLP_HTTP=1)
endif()

target_include_directories(OpenTelemetry
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

POCO_INSTALL(OpenTelemetry)
POCO_GENERATE_PACKAGE(OpenTelemetry)
