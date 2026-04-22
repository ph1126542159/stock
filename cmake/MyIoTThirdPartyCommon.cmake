include_guard(GLOBAL)

function(myiot_normalize_imported_target_locations target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_myiot_is_imported ${target_name} IMPORTED)
    if(NOT _myiot_is_imported)
        return()
    endif()

    get_target_property(_myiot_debug_location ${target_name} IMPORTED_LOCATION_DEBUG)
    get_target_property(_myiot_release_location ${target_name} IMPORTED_LOCATION_RELEASE)
    get_target_property(_myiot_relwithdebinfo_location ${target_name} IMPORTED_LOCATION_RELWITHDEBINFO)
    get_target_property(_myiot_minsizerel_location ${target_name} IMPORTED_LOCATION_MINSIZEREL)

    if(NOT _myiot_release_location)
        set(_myiot_release_location "${_myiot_relwithdebinfo_location}")
    endif()
    if(NOT _myiot_release_location)
        set(_myiot_release_location "${_myiot_minsizerel_location}")
    endif()

    if(_myiot_debug_location)
        set_property(TARGET ${target_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(${target_name} PROPERTIES IMPORTED_LOCATION_DEBUG "${_myiot_debug_location}")
    endif()

    if(_myiot_release_location)
        set_property(TARGET ${target_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION_RELEASE "${_myiot_release_location}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${_myiot_release_location}"
            IMPORTED_LOCATION_MINSIZEREL "${_myiot_release_location}"
        )
    endif()
endfunction()

function(myiot_copy_windows_runtime_dlls target_name)
    if(NOT WIN32 OR NOT TARGET ${target_name})
        return()
    endif()

    # Bootstrap dependencies install runtime DLLs to the shared install/bin prefix.
    # Mirror them into the local target output directory so Windows executables can run in-place.
    file(GLOB _myiot_windows_runtime_dlls
        "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/*.dll"
    )
    list(REMOVE_DUPLICATES _myiot_windows_runtime_dlls)

    foreach(_myiot_runtime_dll IN LISTS _myiot_windows_runtime_dlls)
        if(EXISTS "${_myiot_runtime_dll}")
            get_filename_component(_myiot_runtime_dll_name "${_myiot_runtime_dll}" NAME)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_myiot_runtime_dll}"
                    "$<TARGET_FILE_DIR:${target_name}>/${_myiot_runtime_dll_name}"
                VERBATIM
            )
        endif()
    endforeach()
endfunction()

function(myiot_append_toolchain_cmake_args out_var)
    set(_myiot_args ${ARGN})

    if(CMAKE_C_COMPILER)
        list(APPEND _myiot_args
            -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
        )
    endif()
    if(CMAKE_C_COMPILER_ARG1)
        list(APPEND _myiot_args
            -DCMAKE_C_COMPILER_ARG1:STRING=${CMAKE_C_COMPILER_ARG1}
        )
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND _myiot_args
            -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
        )
    endif()
    if(CMAKE_CXX_COMPILER_ARG1)
        list(APPEND _myiot_args
            -DCMAKE_CXX_COMPILER_ARG1:STRING=${CMAKE_CXX_COMPILER_ARG1}
        )
    endif()
    if(CMAKE_RC_COMPILER)
        list(APPEND _myiot_args
            -DCMAKE_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}
        )
    endif()
    if(DEFINED CMAKE_RC_FLAGS AND NOT CMAKE_RC_FLAGS STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_RC_FLAGS:STRING=${CMAKE_RC_FLAGS}
        )
    endif()
    if(DEFINED CMAKE_MT AND CMAKE_MT AND NOT CMAKE_MT STREQUAL "CMAKE_MT-NOTFOUND")
        list(APPEND _myiot_args
            -DCMAKE_MT:FILEPATH=${CMAKE_MT}
        )
    endif()
    if(CMAKE_MAKE_PROGRAM)
        list(APPEND _myiot_args
            -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
        )
    endif()
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND _myiot_args
            -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}
        )
    endif()
    if(CMAKE_SYSROOT)
        list(APPEND _myiot_args
            -DCMAKE_SYSROOT:PATH=${CMAKE_SYSROOT}
        )
    endif()
    if(NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE)
        list(APPEND _myiot_args
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        )
    endif()

    set(${out_var} "${_myiot_args}" PARENT_SCOPE)
endfunction()

function(myiot_collect_bootstrap_context_definitions out_var)
    set(_myiot_defs
        "MYIOT_BOOTSTRAP_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}"
        "MYIOT_BOOTSTRAP_DEPS_DIR:PATH=${MYIOT_DEPS_DIR}"
        "MYIOT_BOOTSTRAP_CMAKE_DIR:PATH=${CMAKE_SOURCE_DIR}/cmake"
        "MYIOT_BOOTSTRAP_LOG_DIR:PATH=${MYIOT_DEPS_DIR}/logs"
        "MYIOT_BOOTSTRAP_GENERATOR:STRING=${CMAKE_GENERATOR}"
        "MYIOT_BOOTSTRAP_GENERATOR_PLATFORM:STRING=${CMAKE_GENERATOR_PLATFORM}"
        "MYIOT_BOOTSTRAP_GENERATOR_TOOLSET:STRING=${CMAKE_GENERATOR_TOOLSET}"
        "MYIOT_BOOTSTRAP_C_STANDARD:STRING=${CMAKE_C_STANDARD}"
        "MYIOT_BOOTSTRAP_CXX_STANDARD:STRING=${CMAKE_CXX_STANDARD}"
        "MYIOT_BOOTSTRAP_POSITION_INDEPENDENT_CODE:BOOL=${CMAKE_POSITION_INDEPENDENT_CODE}"
        "MYIOT_BOOTSTRAP_PREFIX_PATH:PATH=${CMAKE_INSTALL_PREFIX}"
    )

    if(CMAKE_CONFIGURATION_TYPES)
        string(JOIN "|" _myiot_bootstrap_build_configs ${CMAKE_CONFIGURATION_TYPES})
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_BUILD_CONFIGS:STRING=${_myiot_bootstrap_build_configs}"
        )
    elseif(CMAKE_BUILD_TYPE)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
        )
    endif()

    if(CMAKE_C_COMPILER)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
        )
    endif()
    if(DEFINED CMAKE_C_FLAGS AND NOT CMAKE_C_FLAGS STREQUAL "")
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_C_FLAGS:STRING=${CMAKE_C_FLAGS}"
        )
    endif()
    if(CMAKE_C_COMPILER_ARG1)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_C_COMPILER_ARG1:STRING=${CMAKE_C_COMPILER_ARG1}"
        )
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
        )
    endif()
    if(DEFINED CMAKE_CXX_FLAGS AND NOT CMAKE_CXX_FLAGS STREQUAL "")
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}"
        )
    endif()
    if(CMAKE_CXX_COMPILER_ARG1)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_CXX_COMPILER_ARG1:STRING=${CMAKE_CXX_COMPILER_ARG1}"
        )
    endif()
    if(CMAKE_RC_COMPILER)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}"
        )
    endif()
    if(DEFINED CMAKE_RC_FLAGS AND NOT CMAKE_RC_FLAGS STREQUAL "")
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_RC_FLAGS:STRING=${CMAKE_RC_FLAGS}"
        )
    endif()
    if(DEFINED CMAKE_MT AND CMAKE_MT AND NOT CMAKE_MT STREQUAL "CMAKE_MT-NOTFOUND")
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_MT:FILEPATH=${CMAKE_MT}"
        )
    endif()
    if(CMAKE_MAKE_PROGRAM)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
        )
    endif()
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}"
        )
    endif()
    if(CMAKE_SYSROOT)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_SYSROOT:PATH=${CMAKE_SYSROOT}"
        )
    endif()
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_MSVC_RUNTIME_LIBRARY:STRING=${CMAKE_MSVC_RUNTIME_LIBRARY}"
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_SHOW_PROGRESS)
        list(APPEND _myiot_defs
            "MYIOT_BOOTSTRAP_SHOW_PROGRESS:BOOL=${MYIOT_BOOTSTRAP_SHOW_PROGRESS}"
        )
    endif()

    set(${out_var} "${_myiot_defs}" PARENT_SCOPE)
endfunction()

function(myiot_execute_bootstrap_script script_path)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs DEFINITIONS)
    cmake_parse_arguments(MYIOT_BOOTSTRAP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT EXISTS "${script_path}")
        message(FATAL_ERROR "Bootstrap script not found: ${script_path}")
    endif()

    set(_myiot_command "${CMAKE_COMMAND}")
    foreach(_myiot_definition IN LISTS MYIOT_BOOTSTRAP_DEFINITIONS)
        string(REPLACE ";" "\\;" _myiot_definition_escaped "${_myiot_definition}")
        list(APPEND _myiot_command "-D${_myiot_definition_escaped}")
    endforeach()
    list(APPEND _myiot_command -P "${script_path}")

    execute_process(
        COMMAND ${_myiot_command}
        COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()
