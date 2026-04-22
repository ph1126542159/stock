include_guard(GLOBAL)

set(Poco_FOUND FALSE)
set(_myiot_local_poco_config_dir "${CMAKE_INSTALL_PREFIX}/cmake")
if(WIN32)
    set(_myiot_local_poco_foundation_debug "${CMAKE_INSTALL_PREFIX}/lib/PocoFoundationmdd.lib")
    set(_myiot_local_poco_foundation_release "${CMAKE_INSTALL_PREFIX}/lib/PocoFoundationmd.lib")
    set(_myiot_local_cppunit_debug "${CMAKE_INSTALL_PREFIX}/lib/CppUnitmdd.lib")
    set(_myiot_local_cppunit_release "${CMAKE_INSTALL_PREFIX}/lib/CppUnitmd.lib")
else()
    set(_myiot_local_poco_foundation_debug "${CMAKE_INSTALL_PREFIX}/lib/libPocoFoundationd.a")
    set(_myiot_local_poco_foundation_release "${CMAKE_INSTALL_PREFIX}/lib/libPocoFoundation.a")
    set(_myiot_local_cppunit_debug "${CMAKE_INSTALL_PREFIX}/lib/libCppUnitd.a")
    set(_myiot_local_cppunit_release "${CMAKE_INSTALL_PREFIX}/lib/libCppUnit.a")
endif()

set(_myiot_local_poco_has_required_debug TRUE)
if(WIN32 AND CMAKE_CONFIGURATION_TYPES)
    foreach(_myiot_required_config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOLOWER "${_myiot_required_config}" _myiot_required_config_suffix)
        if(NOT EXISTS "${_myiot_local_poco_config_dir}/PocoFoundationTargets-${_myiot_required_config_suffix}.cmake"
           OR NOT EXISTS "${_myiot_local_poco_config_dir}/PocoCppUnitTargets-${_myiot_required_config_suffix}.cmake")
            set(_myiot_local_poco_has_required_debug FALSE)
            break()
        endif()
    endforeach()

    if(NOT EXISTS "${_myiot_local_poco_foundation_release}"
       OR NOT EXISTS "${_myiot_local_cppunit_release}")
        set(_myiot_local_poco_has_required_debug FALSE)
    endif()
endif()

if(EXISTS "${_myiot_local_poco_config_dir}/PocoConfig.cmake"
   AND EXISTS "${_myiot_local_poco_config_dir}/PocoCppUnitTargets.cmake"
   AND EXISTS "${_myiot_local_poco_foundation_release}"
   AND EXISTS "${_myiot_local_cppunit_release}"
   AND _myiot_local_poco_has_required_debug)
    set(Poco_DIR "${_myiot_local_poco_config_dir}")
    find_package(
        Poco CONFIG QUIET
        COMPONENTS ${MYIOT_POCO_COMPONENTS}
        NO_DEFAULT_PATH
        PATHS "${_myiot_local_poco_config_dir}"
    )

    if(Poco_FOUND)
        message(STATUS "Found official Poco package in install prefix: ${Poco_DIR}")
    endif()
elseif(WIN32 AND CMAKE_CONFIGURATION_TYPES
       AND EXISTS "${_myiot_local_poco_config_dir}/PocoConfig.cmake"
       AND EXISTS "${_myiot_local_poco_foundation_release}")
    message(STATUS
        "Installed Poco package is missing required multi-config metadata under ${CMAKE_INSTALL_PREFIX}. "
        "Rebuilding local Poco package."
    )
endif()

if(NOT Poco_FOUND AND MYIOT_ENABLE_POCO_BOOTSTRAP)
    myiot_collect_bootstrap_context_definitions(_myiot_poco_bootstrap_defs)
    list(APPEND _myiot_poco_bootstrap_defs
        "MYIOT_POCO_SOURCE_DIR:PATH=${MYIOT_POCO_SOURCE_DIR}"
        "MYIOT_POCO_VERSION:STRING=${MYIOT_POCO_VERSION}"
        "MYIOT_POCO_PACKAGE_VERSION:STRING=${MYIOT_POCO_PACKAGE_VERSION}"
        "MYIOT_ENABLE_DATA_MODULES:BOOL=${MYIOT_ENABLE_DATA_MODULES}"
    )
    if(WIN32)
        list(APPEND _myiot_poco_bootstrap_defs
            "MYIOT_BOOTSTRAP_BUILD_PARALLEL_LEVEL:STRING=1"
        )
    endif()
    if(DEFINED MYIOT_OPENSSL_ROOT AND MYIOT_OPENSSL_ROOT)
        list(APPEND _myiot_poco_bootstrap_defs
            "MYIOT_OPENSSL_ROOT_DIR:PATH=${MYIOT_OPENSSL_ROOT}"
        )
    endif()

    myiot_execute_bootstrap_script(
        "${CMAKE_SOURCE_DIR}/cmake/MyIoTPocoBootstrap.cmake"
        DEFINITIONS ${_myiot_poco_bootstrap_defs}
    )

    set(Poco_DIR "${_myiot_local_poco_config_dir}")
    find_package(
        Poco CONFIG REQUIRED
        COMPONENTS ${MYIOT_POCO_COMPONENTS}
        NO_DEFAULT_PATH
        PATHS "${_myiot_local_poco_config_dir}"
    )
elseif(NOT Poco_FOUND)
    message(FATAL_ERROR "Official Poco was not found under ${CMAKE_INSTALL_PREFIX} and MYIOT_ENABLE_POCO_BOOTSTRAP is OFF.")
endif()

if(Poco_FOUND)
    include("${_myiot_local_poco_config_dir}/PocoCppUnitTargets.cmake")
    set_target_properties(Poco::CppUnit PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include"
        INTERFACE_LINK_LIBRARIES "Poco::Foundation"
    )

    foreach(_myiot_poco_target
        Poco::Foundation
        Poco::XML
        Poco::JSON
        Poco::Util
        Poco::Net
        Poco::Crypto
        Poco::NetSSL
        Poco::Zip
        Poco::CppParser
        Poco::Redis
        Poco::JWT
        Poco::CppUnit
    )
        myiot_normalize_imported_target_locations(${_myiot_poco_target})
    endforeach()

    if(MYIOT_ENABLE_DATA_MODULES)
        foreach(_myiot_poco_data_target
            Poco::Data
            Poco::DataSQLite
            Poco::ActiveRecord
        )
            myiot_normalize_imported_target_locations(${_myiot_poco_data_target})
        endforeach()
    endif()
endif()
