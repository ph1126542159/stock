include_guard(GLOBAL)

if(NOT MYIOT_ENABLE_OPENTELEMETRY)
    return()
endif()

function(_myiot_remove_stale_opentelemetry_packages reason_text)
    message(STATUS "${reason_text}")

    foreach(_myiot_package_dir IN ITEMS
        "${CMAKE_INSTALL_PREFIX}/lib/cmake/opentelemetry-cpp"
        "${CMAKE_INSTALL_PREFIX}/lib/cmake/protobuf"
        "${CMAKE_INSTALL_PREFIX}/lib/cmake/absl"
    )
        if(EXISTS "${_myiot_package_dir}")
            file(REMOVE_RECURSE "${_myiot_package_dir}")
        endif()
    endforeach()
endfunction()

set(_myiot_local_opentelemetry_config_dir "${CMAKE_INSTALL_PREFIX}/lib/cmake/opentelemetry-cpp")
if(EXISTS "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-config.cmake"
   AND (
        NOT EXISTS "${_myiot_local_opentelemetry_config_dir}/component-definitions.cmake"
        OR NOT EXISTS "${_myiot_local_opentelemetry_config_dir}/thirdparty-dependency-definitions.cmake"
        OR NOT EXISTS "${_myiot_local_opentelemetry_config_dir}/find-package-support-functions.cmake"
   ))
    _myiot_remove_stale_opentelemetry_packages(
        "Removing incomplete OpenTelemetry C++ package metadata from install prefix: ${_myiot_local_opentelemetry_config_dir}"
    )
endif()

if(EXISTS "${_myiot_local_opentelemetry_config_dir}/component-definitions.cmake")
    set(_myiot_required_local_opentelemetry_components)
    if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
        list(APPEND _myiot_required_local_opentelemetry_components exporters_otlp_http)
    endif()

    if(_myiot_required_local_opentelemetry_components)
        file(READ
            "${_myiot_local_opentelemetry_config_dir}/component-definitions.cmake"
            _myiot_local_opentelemetry_component_definitions
        )

        foreach(_myiot_required_component IN LISTS _myiot_required_local_opentelemetry_components)
            string(FIND
                "${_myiot_local_opentelemetry_component_definitions}"
                "${_myiot_required_component}"
                _myiot_required_component_index
            )
            if(_myiot_required_component_index EQUAL -1)
                _myiot_remove_stale_opentelemetry_packages(
                    "Installed OpenTelemetry package is missing requested component ${_myiot_required_component}. Removing local package metadata so it can be rebuilt."
                )
                break()
            endif()
        endforeach()
    endif()
endif()

if(EXISTS "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-sdk-target-debug.cmake"
   AND EXISTS "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-sdk-target-release.cmake")
    file(READ
        "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-sdk-target-debug.cmake"
        _myiot_local_opentelemetry_debug_targets
    )
    file(READ
        "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-sdk-target-release.cmake"
        _myiot_local_opentelemetry_release_targets
    )

    string(FIND
        "${_myiot_local_opentelemetry_debug_targets}"
        "IMPORTED_LOCATION_DEBUG \"\${_IMPORT_PREFIX}/lib/opentelemetry_common.lib\""
        _myiot_opentelemetry_debug_unsuffixed_index
    )
    string(FIND
        "${_myiot_local_opentelemetry_release_targets}"
        "IMPORTED_LOCATION_RELEASE \"\${_IMPORT_PREFIX}/lib/opentelemetry_common.lib\""
        _myiot_opentelemetry_release_unsuffixed_index
    )

    if(NOT _myiot_opentelemetry_debug_unsuffixed_index EQUAL -1
       AND NOT _myiot_opentelemetry_release_unsuffixed_index EQUAL -1)
        _myiot_remove_stale_opentelemetry_packages(
            "Installed OpenTelemetry/Protobuf/Abseil package metadata reuses the same library filenames for Debug and Release. Removing stale metadata so bootstrap can rebuild them with configuration-specific postfixes."
        )
    endif()
endif()

set(_myiot_opentelemetry_core_components
    api
    sdk
)
set(_myiot_opentelemetry_optional_components)
set(_myiot_opentelemetry_imported_targets
    opentelemetry-cpp::api
    opentelemetry-cpp::sdk
    opentelemetry-cpp::version
    opentelemetry-cpp::common
    opentelemetry-cpp::resources
    opentelemetry-cpp::trace
    opentelemetry-cpp::metrics
    opentelemetry-cpp::logs
)

if(MYIOT_OPENTELEMETRY_ENABLE_OSTREAM_EXPORTER)
    list(APPEND _myiot_opentelemetry_optional_components exporters_ostream)
    list(APPEND _myiot_opentelemetry_imported_targets
        opentelemetry-cpp::ostream_span_exporter
        opentelemetry-cpp::ostream_metrics_exporter
        opentelemetry-cpp::ostream_log_record_exporter
    )
endif()

if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
    list(APPEND _myiot_opentelemetry_optional_components resource_detectors)
    list(APPEND _myiot_opentelemetry_imported_targets
        opentelemetry-cpp::resource_detectors
    )
endif()

if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
    list(APPEND _myiot_opentelemetry_optional_components exporters_otlp_http)
    list(APPEND _myiot_opentelemetry_imported_targets
        opentelemetry-cpp::otlp_http_client
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::otlp_http_log_record_exporter
        opentelemetry-cpp::otlp_http_metric_exporter
    )
endif()

if(EXISTS "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-config.cmake")
    set(opentelemetry-cpp_DIR "${_myiot_local_opentelemetry_config_dir}")
    find_package(
        opentelemetry-cpp CONFIG QUIET
        COMPONENTS ${_myiot_opentelemetry_core_components}
        NO_DEFAULT_PATH
        PATHS "${_myiot_local_opentelemetry_config_dir}"
    )
endif()

if(TARGET opentelemetry-cpp::api AND TARGET opentelemetry-cpp::sdk)
    message(STATUS "Found OpenTelemetry C++ package in install prefix")
    if(_myiot_opentelemetry_optional_components)
        find_package(
            opentelemetry-cpp CONFIG QUIET
            COMPONENTS ${_myiot_opentelemetry_optional_components}
            NO_DEFAULT_PATH
            PATHS "${_myiot_local_opentelemetry_config_dir}"
        )
    endif()
elseif(MYIOT_ENABLE_OPENTELEMETRY_BOOTSTRAP)
    myiot_collect_bootstrap_context_definitions(_myiot_opentelemetry_bootstrap_defs)
    list(APPEND _myiot_opentelemetry_bootstrap_defs
        "MYIOT_OPENTELEMETRY_SOURCE_DIR:PATH=${MYIOT_OPENTELEMETRY_SOURCE_DIR}"
        "MYIOT_OPENTELEMETRY_VERSION:STRING=${MYIOT_OPENTELEMETRY_VERSION}"
        "MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER:BOOL=${MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER}"
        "MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS:BOOL=${MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS}"
    )

    myiot_execute_bootstrap_script(
        "${CMAKE_SOURCE_DIR}/cmake/MyIoTOpenTelemetryBootstrap.cmake"
        DEFINITIONS ${_myiot_opentelemetry_bootstrap_defs}
    )

    set(opentelemetry-cpp_DIR "${_myiot_local_opentelemetry_config_dir}")
    find_package(
        opentelemetry-cpp CONFIG REQUIRED
        COMPONENTS ${_myiot_opentelemetry_core_components}
        NO_DEFAULT_PATH
        PATHS "${_myiot_local_opentelemetry_config_dir}"
    )
    if(_myiot_opentelemetry_optional_components)
        find_package(
            opentelemetry-cpp CONFIG QUIET
            COMPONENTS ${_myiot_opentelemetry_optional_components}
            NO_DEFAULT_PATH
            PATHS "${_myiot_local_opentelemetry_config_dir}"
        )
    endif()
else()
    message(FATAL_ERROR
        "OpenTelemetry C++ was not found under ${CMAKE_INSTALL_PREFIX} and "
        "MYIOT_ENABLE_OPENTELEMETRY_BOOTSTRAP is OFF."
    )
endif()

foreach(_myiot_opentelemetry_target IN LISTS _myiot_opentelemetry_imported_targets)
    myiot_normalize_imported_target_locations(${_myiot_opentelemetry_target})
endforeach()

add_library(myiot_opentelemetry INTERFACE)
target_link_libraries(myiot_opentelemetry
    INTERFACE
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::trace
        opentelemetry-cpp::metrics
        opentelemetry-cpp::logs
        opentelemetry-cpp::resources
)

if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
    if(TARGET opentelemetry-cpp::resource_detectors)
        add_library(myiot_opentelemetry_resource_detectors INTERFACE)
        target_link_libraries(myiot_opentelemetry_resource_detectors
            INTERFACE
                myiot_opentelemetry
                opentelemetry-cpp::resource_detectors
        )
    else()
        message(WARNING
            "OpenTelemetry resource detectors were requested, but the installed package does not provide "
            "the resource_detectors component."
        )
    endif()
endif()

if(MYIOT_OPENTELEMETRY_ENABLE_OSTREAM_EXPORTER)
    if(TARGET opentelemetry-cpp::ostream_span_exporter)
        add_library(myiot_opentelemetry_ostream INTERFACE)
        target_link_libraries(myiot_opentelemetry_ostream
            INTERFACE
                myiot_opentelemetry
                opentelemetry-cpp::ostream_span_exporter
        )

        if(TARGET opentelemetry-cpp::ostream_metrics_exporter)
            target_link_libraries(myiot_opentelemetry_ostream
                INTERFACE
                    opentelemetry-cpp::ostream_metrics_exporter
            )
        endif()

        if(TARGET opentelemetry-cpp::ostream_log_record_exporter)
            target_link_libraries(myiot_opentelemetry_ostream
                INTERFACE
                    opentelemetry-cpp::ostream_log_record_exporter
            )
        endif()
    else()
        message(WARNING
            "OpenTelemetry ostream exporters were requested, but the installed package does not provide "
            "the exporters_ostream component."
        )
    endif()
endif()

if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
    if(TARGET opentelemetry-cpp::otlp_http_exporter
       AND TARGET opentelemetry-cpp::otlp_http_log_record_exporter
       AND TARGET opentelemetry-cpp::otlp_http_metric_exporter
       AND TARGET opentelemetry-cpp::otlp_http_client)
        add_library(myiot_opentelemetry_otlp_http INTERFACE)
        target_link_libraries(myiot_opentelemetry_otlp_http
            INTERFACE
                myiot_opentelemetry
                opentelemetry-cpp::otlp_http_client
                opentelemetry-cpp::otlp_http_exporter
                opentelemetry-cpp::otlp_http_log_record_exporter
                opentelemetry-cpp::otlp_http_metric_exporter
        )
    else()
        message(WARNING
            "OpenTelemetry OTLP HTTP exporters were requested, but the installed package does not provide "
            "the exporters_otlp_http component."
        )
    endif()
endif()
