cmake_minimum_required(VERSION 3.20)

project(MyIoTPocoBootstrap LANGUAGES C CXX RC)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(NOT DEFINED MYIOT_POCO_SOURCE_DIR OR NOT EXISTS "${MYIOT_POCO_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "MYIOT_POCO_SOURCE_DIR must point to a valid Poco source tree.")
endif()
if(NOT DEFINED MYIOT_POCO_PACKAGE_VERSION OR MYIOT_POCO_PACKAGE_VERSION STREQUAL "")
    message(FATAL_ERROR "MYIOT_POCO_PACKAGE_VERSION is required.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC AND DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif()

if(CMAKE_RC_COMPILER)
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_RC_COMPILER
        CMAKE_RC_FLAGS
    )
endif()
if(DEFINED CMAKE_MT AND CMAKE_MT AND NOT CMAKE_MT STREQUAL "CMAKE_MT-NOTFOUND")
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_MT)
endif()
list(REMOVE_DUPLICATES CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)

if(MSVC)
    # Poco builds many translation units into the same PDB in RelWithDebInfo.
    # /FS serializes access to the compiler's shared PDB writer and avoids C1041.
    add_compile_options(/FS)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(POCO_UNBUNDLED OFF CACHE BOOL "Use external third-party dependencies" FORCE)
set(POCO_DISABLE_INTERNAL_OPENSSL ON CACHE BOOL "Use externally provided OpenSSL" FORCE)
set(DISABLE_CPP11 OFF CACHE BOOL "Disable C++11 support" FORCE)
set(DISABLE_CPP14 OFF CACHE BOOL "Disable C++14 support" FORCE)
set(MYSQL_FOUND OFF CACHE BOOL "Disable MySQL support" FORCE)
set(POSTGRESQL_FOUND OFF CACHE BOOL "Disable PostgreSQL support" FORCE)
set(ODBC_FOUND OFF CACHE BOOL "Disable ODBC support" FORCE)
set(ENABLE_DATA_MYSQL OFF CACHE BOOL "Disable Poco Data MySQL support" FORCE)
set(ENABLE_DATA_POSTGRESQL OFF CACHE BOOL "Disable Poco Data PostgreSQL support" FORCE)
set(ENABLE_DATA_ODBC OFF CACHE BOOL "Disable Poco Data ODBC support" FORCE)
set(SHARED_LIBRARY_VERSION 1 CACHE STRING "Compatibility version for imported POCO modules" FORCE)

set(ENABLE_FOUNDATION ON CACHE BOOL "Enable Foundation" FORCE)
set(ENABLE_ENCODINGS OFF CACHE BOOL "Disable Encodings" FORCE)
set(ENABLE_ENCODINGS_COMPILER OFF CACHE BOOL "Disable Encodings Compiler" FORCE)
set(ENABLE_XML ON CACHE BOOL "Enable XML" FORCE)
set(ENABLE_JSON ON CACHE BOOL "Enable JSON" FORCE)
set(ENABLE_MONGODB OFF CACHE BOOL "Disable MongoDB" FORCE)
if(MYIOT_ENABLE_DATA_MODULES)
    set(ENABLE_DATA ON CACHE BOOL "Enable Data" FORCE)
    set(ENABLE_DATA_SQLITE ON CACHE BOOL "Enable Data SQLite" FORCE)
    set(ENABLE_ACTIVERECORD ON CACHE BOOL "Enable ActiveRecord" FORCE)
else()
    set(ENABLE_DATA OFF CACHE BOOL "Disable Data" FORCE)
    set(ENABLE_DATA_SQLITE OFF CACHE BOOL "Disable Data SQLite" FORCE)
    set(ENABLE_ACTIVERECORD OFF CACHE BOOL "Disable ActiveRecord" FORCE)
endif()
set(ENABLE_REDIS ON CACHE BOOL "Enable Redis" FORCE)
set(ENABLE_PDF OFF CACHE BOOL "Disable PDF" FORCE)
set(ENABLE_UTIL ON CACHE BOOL "Enable Util" FORCE)
set(ENABLE_NET ON CACHE BOOL "Enable Net" FORCE)
set(ENABLE_SEVENZIP OFF CACHE BOOL "Disable SevenZip" FORCE)
set(ENABLE_ZIP ON CACHE BOOL "Enable Zip" FORCE)
set(ENABLE_CPPPARSER ON CACHE BOOL "Enable CppParser" FORCE)
set(ENABLE_POCODOC OFF CACHE BOOL "Disable PocoDoc" FORCE)
set(ENABLE_PAGECOMPILER OFF CACHE BOOL "Disable PageCompiler" FORCE)
set(ENABLE_PAGECOMPILER_FILE2PAGE OFF CACHE BOOL "Disable File2Page" FORCE)
set(ENABLE_ACTIVERECORD_COMPILER OFF CACHE BOOL "Disable ActiveRecord compiler" FORCE)
set(ENABLE_NETSSL_WIN OFF CACHE BOOL "Disable NetSSL_Win" FORCE)
set(ENABLE_NETSSL ON CACHE BOOL "Enable NetSSL" FORCE)
set(ENABLE_CRYPTO ON CACHE BOOL "Enable Crypto" FORCE)
set(ENABLE_JWT ON CACHE BOOL "Enable JWT" FORCE)
set(ENABLE_APACHECONNECTOR OFF CACHE BOOL "Disable ApacheConnector" FORCE)
set(ENABLE_TESTS OFF CACHE BOOL "Disable official Poco tests and samples" FORCE)

add_subdirectory("${MYIOT_POCO_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/poco-official")
add_subdirectory("${MYIOT_POCO_SOURCE_DIR}/CppUnit" "${CMAKE_BINARY_DIR}/poco-official/CppUnit")

set(_myiot_cppunit_config_dir "${CMAKE_BINARY_DIR}/myiot-poco-cppunit")
file(MAKE_DIRECTORY "${_myiot_cppunit_config_dir}")
file(WRITE "${_myiot_cppunit_config_dir}/PocoCppUnitConfig.cmake"
    "include(\"${CMAKE_CURRENT_LIST_DIR}/PocoCppUnitTargets.cmake\")\n"
)
write_basic_package_version_file(
    "${_myiot_cppunit_config_dir}/PocoCppUnitConfigVersion.cmake"
    VERSION "${MYIOT_POCO_PACKAGE_VERSION}"
    COMPATIBILITY AnyNewerVersion
)

install(DIRECTORY "${MYIOT_POCO_SOURCE_DIR}/CppUnit/include/CppUnit"
    DESTINATION include
    COMPONENT Devel
)
install(TARGETS CppUnit EXPORT PocoCppUnitTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT PocoCppUnitTargets
    FILE PocoCppUnitTargets.cmake
    NAMESPACE Poco::
    DESTINATION cmake
)
install(FILES
    "${_myiot_cppunit_config_dir}/PocoCppUnitConfig.cmake"
    "${_myiot_cppunit_config_dir}/PocoCppUnitConfigVersion.cmake"
    DESTINATION cmake
)
