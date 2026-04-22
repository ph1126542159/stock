include("${CMAKE_CURRENT_LIST_DIR}/MyIoTBootstrapHelpers.cmake")

myiot_bootstrap_require(MYIOT_BOOTSTRAP_DEPS_DIR)
myiot_bootstrap_require(MYIOT_BOOTSTRAP_CMAKE_DIR)
myiot_bootstrap_require(MYIOT_POCO_SOURCE_DIR)
myiot_bootstrap_require(MYIOT_POCO_VERSION)
myiot_bootstrap_require(MYIOT_POCO_PACKAGE_VERSION)

myiot_bootstrap_clone_if_missing(
    "${MYIOT_POCO_SOURCE_DIR}"
    "https://github.com/pocoproject/poco.git"
    "${MYIOT_POCO_VERSION}"
    "Poco"
)

set(_myiot_poco_wrapper_dir "${MYIOT_BOOTSTRAP_CMAKE_DIR}/poco-bootstrap")
if(NOT EXISTS "${_myiot_poco_wrapper_dir}/CMakeLists.txt")
    message(FATAL_ERROR "Poco bootstrap wrapper project was not found: ${_myiot_poco_wrapper_dir}")
endif()

set(_myiot_poco_args
    -DMYIOT_POCO_SOURCE_DIR:PATH=${MYIOT_POCO_SOURCE_DIR}
    -DMYIOT_POCO_PACKAGE_VERSION:STRING=${MYIOT_POCO_PACKAGE_VERSION}
    -DMYIOT_ENABLE_DATA_MODULES:BOOL=${MYIOT_ENABLE_DATA_MODULES}
)

if(DEFINED MYIOT_OPENSSL_ROOT_DIR AND NOT MYIOT_OPENSSL_ROOT_DIR STREQUAL "")
    list(APPEND _myiot_poco_args
        -DOPENSSL_ROOT_DIR:PATH=${MYIOT_OPENSSL_ROOT_DIR}
    )
endif()

myiot_bootstrap_configure_build_install(
    "Poco"
    "${_myiot_poco_wrapper_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/Poco-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_poco_args}
)
