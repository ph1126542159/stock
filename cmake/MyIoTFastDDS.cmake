include_guard(GLOBAL)

set(MYIOT_FASTDDS_PREFIX
    "${CMAKE_INSTALL_PREFIX}"
    CACHE PATH "Install prefix used to discover or bootstrap Fast-DDS")

set(_myiot_fastdds_config_dir "${MYIOT_FASTDDS_PREFIX}/share/fastdds/cmake")
set(_myiot_fastcdr_config_dir "${MYIOT_FASTDDS_PREFIX}/lib/cmake/fastcdr")
set(_myiot_foonathan_config_dir "${MYIOT_FASTDDS_PREFIX}/lib/foonathan_memory/cmake")

if(EXISTS "${_myiot_fastdds_config_dir}/fastdds-config.cmake")
    set(fastdds_DIR "${_myiot_fastdds_config_dir}")
endif()
if(EXISTS "${_myiot_fastcdr_config_dir}/fastcdr-config.cmake")
    set(fastcdr_DIR "${_myiot_fastcdr_config_dir}")
endif()
if(EXISTS "${_myiot_foonathan_config_dir}/foonathan_memory-config.cmake")
    set(foonathan_memory_DIR "${_myiot_foonathan_config_dir}")
endif()

find_package(fastdds CONFIG QUIET NO_DEFAULT_PATH PATHS "${_myiot_fastdds_config_dir}")

if(fastdds_FOUND)
    message(STATUS "Found Fast-DDS package in install prefix: ${fastdds_DIR}")
elseif(MYIOT_ENABLE_FASTDDS_BOOTSTRAP)
    myiot_collect_bootstrap_context_definitions(_myiot_fastdds_bootstrap_defs)
    list(APPEND _myiot_fastdds_bootstrap_defs
        "MYIOT_BOOTSTRAP_INSTALL_PREFIX:PATH=${MYIOT_FASTDDS_PREFIX}"
        "MYIOT_BOOTSTRAP_PREFIX_PATH:PATH=${MYIOT_FASTDDS_PREFIX}"
        "MYIOT_FASTDDS_PREFIX:PATH=${MYIOT_FASTDDS_PREFIX}"
        "MYIOT_FASTDDS_VERSION:STRING=${MYIOT_FASTDDS_VERSION}"
        "MYIOT_FASTCDR_VERSION:STRING=${MYIOT_FASTCDR_VERSION}"
        "MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION:STRING=${MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION}"
    )

    myiot_execute_bootstrap_script(
        "${CMAKE_SOURCE_DIR}/cmake/MyIoTFastDDSBootstrap.cmake"
        DEFINITIONS ${_myiot_fastdds_bootstrap_defs}
    )

    set(fastdds_DIR "${_myiot_fastdds_config_dir}")
    set(fastcdr_DIR "${_myiot_fastcdr_config_dir}")
    set(foonathan_memory_DIR "${_myiot_foonathan_config_dir}")
    find_package(fastdds CONFIG REQUIRED NO_DEFAULT_PATH PATHS "${_myiot_fastdds_config_dir}")
else()
    message(FATAL_ERROR
        "Fast-DDS was not found under ${MYIOT_FASTDDS_PREFIX} and MYIOT_ENABLE_FASTDDS_BOOTSTRAP is OFF."
    )
endif()
