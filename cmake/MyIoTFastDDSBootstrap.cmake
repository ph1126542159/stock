include("${CMAKE_CURRENT_LIST_DIR}/MyIoTBootstrapHelpers.cmake")

function(myiot_prepare_foonathan_memory_vendor_source source_dir)
    if(NOT WIN32)
        return()
    endif()

    set(_vendor_cmakelists "${source_dir}/CMakeLists.txt")
    if(NOT EXISTS "${_vendor_cmakelists}")
        message(FATAL_ERROR "foonathan_memory_vendor CMakeLists.txt not found: ${_vendor_cmakelists}")
    endif()

    file(TO_CMAKE_PATH
        "${CMAKE_CURRENT_LIST_DIR}/PatchFoonathanGetContainerNodeSizes.cmake"
        _myiot_patch_script
    )

    file(READ "${_vendor_cmakelists}" _vendor_contents)
    if(_vendor_contents MATCHES "MYIOT_PATCH_FOONATHAN_GET_CONTAINER_NODE_SIZES")
        return()
    endif()

    set(_vendor_old "  UPDATE_COMMAND \"\"\n  CMAKE_ARGS")
    set(_vendor_new
"  UPDATE_COMMAND \"\"
  PATCH_COMMAND
    \"${CMAKE_COMMAND}\" -DINPUT_FILE=<SOURCE_DIR>/cmake/get_container_node_sizes.cmake -P \"${_myiot_patch_script}\" # MYIOT_PATCH_FOONATHAN_GET_CONTAINER_NODE_SIZES
  CMAKE_ARGS")

    string(FIND "${_vendor_contents}" "${_vendor_old}" _vendor_patch_pos)
    if(_vendor_patch_pos EQUAL -1)
        message(FATAL_ERROR
            "Unable to inject foonathan_memory patch command into ${_vendor_cmakelists}."
        )
    endif()

    string(REPLACE "${_vendor_old}" "${_vendor_new}" _vendor_contents "${_vendor_contents}")
    file(WRITE "${_vendor_cmakelists}" "${_vendor_contents}")
endfunction()

myiot_bootstrap_require(MYIOT_BOOTSTRAP_DEPS_DIR)
myiot_bootstrap_require(MYIOT_FASTDDS_PREFIX)
myiot_bootstrap_require(MYIOT_FASTDDS_VERSION)
myiot_bootstrap_require(MYIOT_FASTCDR_VERSION)
myiot_bootstrap_require(MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION)

set(_myiot_foonathan_source_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/foonathan_memory_vendor")
set(_myiot_fastcdr_source_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-CDR")
set(_myiot_fastdds_source_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-DDS")

myiot_bootstrap_clone_if_missing(
    "${_myiot_foonathan_source_dir}"
    "https://github.com/eProsima/foonathan_memory_vendor.git"
    "${MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION}"
    "foonathan_memory_vendor"
)
myiot_prepare_foonathan_memory_vendor_source("${_myiot_foonathan_source_dir}")
myiot_bootstrap_clone_if_missing(
    "${_myiot_fastcdr_source_dir}"
    "https://github.com/eProsima/Fast-CDR.git"
    "${MYIOT_FASTCDR_VERSION}"
    "Fast-CDR"
)
myiot_bootstrap_clone_if_missing(
    "${_myiot_fastdds_source_dir}"
    "https://github.com/eProsima/Fast-DDS.git"
    "${MYIOT_FASTDDS_VERSION}"
    "Fast-DDS"
)
myiot_bootstrap_update_submodules(
    "${_myiot_fastdds_source_dir}"
    "Fast-DDS"
    PATHS
        thirdparty/android-ifaddrs
        thirdparty/asio
        thirdparty/fastcdr
        thirdparty/tinyxml2
)

set(_myiot_fastdds_common_args
    -DBUILD_TESTING:BOOL=OFF
)

if(WIN32)
    list(APPEND _myiot_fastdds_common_args
        -DEPROSIMA_EXTRA_CMAKE_CXX_FLAGS:STRING=/utf-8
    )
endif()

myiot_bootstrap_configure_build_install(
    "foonathan_memory_vendor"
    "${_myiot_foonathan_source_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/foonathan_memory_vendor-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_fastdds_common_args}
)
myiot_bootstrap_configure_build_install(
    "Fast-CDR"
    "${_myiot_fastcdr_source_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-CDR-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_fastdds_common_args}
)
myiot_bootstrap_configure_build_install(
    "Fast-DDS"
    "${_myiot_fastdds_source_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-DDS-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_fastdds_common_args}
        -DTHIRDPARTY:STRING=ON
        -DTHIRDPARTY_Asio:STRING=ON
        -DTHIRDPARTY_TinyXML2:STRING=ON
        -DTHIRDPARTY_UPDATE:BOOL=OFF
)
