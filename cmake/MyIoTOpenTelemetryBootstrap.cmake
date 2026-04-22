include("${CMAKE_CURRENT_LIST_DIR}/MyIoTBootstrapHelpers.cmake")

myiot_bootstrap_require(MYIOT_BOOTSTRAP_DEPS_DIR)
myiot_bootstrap_require(MYIOT_OPENTELEMETRY_SOURCE_DIR)
myiot_bootstrap_require(MYIOT_OPENTELEMETRY_VERSION)

myiot_bootstrap_clone_if_missing(
    "${MYIOT_OPENTELEMETRY_SOURCE_DIR}"
    "https://github.com/open-telemetry/opentelemetry-cpp.git"
    "${MYIOT_OPENTELEMETRY_VERSION}"
    "OpenTelemetry C++"
)

set(_myiot_opentelemetry_args
    -DOPENTELEMETRY_INSTALL:BOOL=ON
    -DWITH_API_ONLY:BOOL=OFF
    -DWITH_ABI_VERSION_1:BOOL=ON
    -DWITH_ABI_VERSION_2:BOOL=OFF
    -DWITH_CONFIGURATION:BOOL=OFF
    -DWITH_OTLP_GRPC:BOOL=OFF
    -DWITH_OTLP_FILE:BOOL=OFF
    -DWITH_OTLP_HTTP_COMPRESSION:BOOL=OFF
    -DWITH_ZIPKIN:BOOL=OFF
    -DWITH_PROMETHEUS:BOOL=OFF
    -DWITH_ELASTICSEARCH:BOOL=OFF
    -DWITH_OPENTRACING:BOOL=OFF
    -DWITH_EXAMPLES:BOOL=OFF
    -DWITH_EXAMPLES_HTTP:BOOL=OFF
    -DWITH_FUNC_TESTS:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF
    -DWITH_BENCHMARK:BOOL=OFF
    -DCMAKE_DISABLE_FIND_PACKAGE_Protobuf:BOOL=ON
)

if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
    list(APPEND _myiot_opentelemetry_args
        -DWITH_OTLP_HTTP:BOOL=ON
    )
else()
    list(APPEND _myiot_opentelemetry_args
        -DWITH_OTLP_HTTP:BOOL=OFF
    )
endif()

if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
    list(APPEND _myiot_opentelemetry_args
        -DWITH_RESOURCE_DETECTORS_PREVIEW:BOOL=ON
    )
else()
    list(APPEND _myiot_opentelemetry_args
        -DWITH_RESOURCE_DETECTORS_PREVIEW:BOOL=OFF
    )
endif()

if(WIN32)
    list(APPEND _myiot_opentelemetry_args
        -DWITH_ETW:BOOL=OFF
        -Dprotobuf_MSVC_STATIC_RUNTIME:BOOL=OFF
        -DABSL_MSVC_STATIC_RUNTIME:BOOL=OFF
        -DCURL_STATIC_CRT:BOOL=OFF
        -Dprotobuf_FORCE_FETCH_DEPENDENCIES:BOOL=ON
        -DCMAKE_DEBUG_POSTFIX:STRING=d
        -DCMAKE_RELWITHDEBINFO_POSTFIX:STRING=rd
        -DCMAKE_MINSIZEREL_POSTFIX:STRING=s
    )
endif()

myiot_bootstrap_configure_build_install(
    "OpenTelemetry C++"
    "${MYIOT_OPENTELEMETRY_SOURCE_DIR}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/opentelemetry-cpp-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_opentelemetry_args}
)
