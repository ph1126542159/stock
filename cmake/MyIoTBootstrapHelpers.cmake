include_guard(GLOBAL)

function(myiot_bootstrap_require variable_name)
    if(NOT DEFINED ${variable_name} OR "${${variable_name}}" STREQUAL "")
        message(FATAL_ERROR "Required bootstrap variable is missing: ${variable_name}")
    endif()
endfunction()

function(myiot_bootstrap_run_logged_result result_var log_file_var description log_name)
    set(options)
    set(oneValueArgs WORKING_DIRECTORY)
    set(multiValueArgs COMMAND)
    cmake_parse_arguments(MYIOT_RUN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT MYIOT_RUN_COMMAND)
        message(FATAL_ERROR "Bootstrap command logging requires COMMAND.")
    endif()

    myiot_bootstrap_require(MYIOT_BOOTSTRAP_LOG_DIR)
    file(MAKE_DIRECTORY "${MYIOT_BOOTSTRAP_LOG_DIR}")

    set(_myiot_log_file "${MYIOT_BOOTSTRAP_LOG_DIR}/${log_name}.log")
    file(REMOVE "${_myiot_log_file}")
    string(REPLACE ";" " " _myiot_command_text "${MYIOT_RUN_COMMAND}")
    file(WRITE "${_myiot_log_file}"
        "Description: ${description}\n"
        "Working directory: ${MYIOT_RUN_WORKING_DIRECTORY}\n"
        "Command: ${_myiot_command_text}\n\n"
    )

    message(STATUS "${description}")

    if(DEFINED MYIOT_BOOTSTRAP_SHOW_PROGRESS AND MYIOT_BOOTSTRAP_SHOW_PROGRESS)
        if(MYIOT_RUN_WORKING_DIRECTORY)
            execute_process(
                COMMAND ${MYIOT_RUN_COMMAND}
                WORKING_DIRECTORY "${MYIOT_RUN_WORKING_DIRECTORY}"
                RESULT_VARIABLE _myiot_result
                COMMAND_ECHO STDOUT
            )
        else()
            execute_process(
                COMMAND ${MYIOT_RUN_COMMAND}
                RESULT_VARIABLE _myiot_result
                COMMAND_ECHO STDOUT
            )
        endif()
        file(APPEND "${_myiot_log_file}"
            "Output was streamed live to the console because "
            "MYIOT_BOOTSTRAP_SHOW_PROGRESS is ON.\n"
        )
    else()
        if(MYIOT_RUN_WORKING_DIRECTORY)
            execute_process(
                COMMAND ${MYIOT_RUN_COMMAND}
                WORKING_DIRECTORY "${MYIOT_RUN_WORKING_DIRECTORY}"
                RESULT_VARIABLE _myiot_result
                OUTPUT_FILE "${_myiot_log_file}"
                ERROR_FILE "${_myiot_log_file}"
            )
        else()
            execute_process(
                COMMAND ${MYIOT_RUN_COMMAND}
                RESULT_VARIABLE _myiot_result
                OUTPUT_FILE "${_myiot_log_file}"
                ERROR_FILE "${_myiot_log_file}"
            )
        endif()
    endif()

    file(APPEND "${_myiot_log_file}" "\nExit code: ${_myiot_result}\n")

    set(${result_var} "${_myiot_result}" PARENT_SCOPE)
    set(${log_file_var} "${_myiot_log_file}" PARENT_SCOPE)
endfunction()

function(myiot_bootstrap_run_logged description log_name)
    myiot_bootstrap_run_logged_result(
        _myiot_result
        _myiot_log_file
        "${description}"
        "${log_name}"
        ${ARGN}
    )

    if(NOT _myiot_result EQUAL 0)
        message(FATAL_ERROR
            "${description} failed with exit code ${_myiot_result}. "
            "See log: ${_myiot_log_file}"
        )
    endif()
endfunction()

function(myiot_bootstrap_make_progress_description out_var step_index step_count description)
    if("${step_count}" MATCHES "^[0-9]+$" AND step_count GREATER 0)
        set(${out_var} "[${step_index}/${step_count}] ${description}" PARENT_SCOPE)
    else()
        set(${out_var} "${description}" PARENT_SCOPE)
    endif()
endfunction()

function(myiot_bootstrap_remove_incomplete_source_tree source_dir display_name)
    if(NOT EXISTS "${source_dir}")
        return()
    endif()

    set(_myiot_source_dir_path "${source_dir}")
    set(_myiot_deps_dir_path "${MYIOT_BOOTSTRAP_DEPS_DIR}")
    cmake_path(ABSOLUTE_PATH _myiot_source_dir_path NORMALIZE OUTPUT_VARIABLE _myiot_source_dir_abs)
    cmake_path(ABSOLUTE_PATH _myiot_deps_dir_path NORMALIZE OUTPUT_VARIABLE _myiot_deps_dir_abs)
    cmake_path(IS_PREFIX _myiot_deps_dir_abs "${_myiot_source_dir_abs}" NORMALIZE _myiot_source_dir_in_deps)

    if(NOT _myiot_source_dir_in_deps)
        message(FATAL_ERROR
            "${display_name} source directory exists but does not look like a valid checkout: "
            "${source_dir}. Remove it manually or point the corresponding *_SOURCE_DIR cache entry "
            "at a valid source tree."
        )
    endif()

    message(STATUS "Removing incomplete ${display_name} source tree: ${source_dir}")
    file(REMOVE_RECURSE "${source_dir}")
endfunction()

function(myiot_bootstrap_try_download_github_archive
    success_var
    log_file_var
    source_dir
    repository
    tag
    display_name
    log_name
)
    myiot_bootstrap_require(MYIOT_BOOTSTRAP_LOG_DIR)
    myiot_bootstrap_require(MYIOT_BOOTSTRAP_DEPS_DIR)

    file(MAKE_DIRECTORY "${MYIOT_BOOTSTRAP_LOG_DIR}")
    set(_myiot_log_file "${MYIOT_BOOTSTRAP_LOG_DIR}/${log_name}.log")
    file(REMOVE "${_myiot_log_file}")
    file(WRITE "${_myiot_log_file}"
        "Description: Downloading ${display_name} ${tag} from a GitHub source archive\n"
        "Repository: ${repository}\n"
        "Destination: ${source_dir}\n\n"
    )

    set(${success_var} FALSE PARENT_SCOPE)
    set(${log_file_var} "${_myiot_log_file}" PARENT_SCOPE)

    if(NOT "${repository}" MATCHES "^https?://github\\.com/([^/]+)/([^/]+)/?$")
        file(APPEND "${_myiot_log_file}"
            "Archive fallback is only implemented for GitHub HTTPS repositories.\n"
        )
        return()
    endif()

    set(_myiot_repo_owner "${CMAKE_MATCH_1}")
    set(_myiot_repo_name "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "\\.git$" "" _myiot_repo_name "${_myiot_repo_name}")

    set(_myiot_archive_urls
        "https://codeload.github.com/${_myiot_repo_owner}/${_myiot_repo_name}/zip/refs/tags/${tag}"
        "https://github.com/${_myiot_repo_owner}/${_myiot_repo_name}/archive/refs/tags/${tag}.zip"
        "https://codeload.github.com/${_myiot_repo_owner}/${_myiot_repo_name}/zip/refs/heads/${tag}"
        "https://github.com/${_myiot_repo_owner}/${_myiot_repo_name}/archive/refs/heads/${tag}.zip"
    )
    list(LENGTH _myiot_archive_urls _myiot_archive_attempt_count)

    string(MAKE_C_IDENTIFIER "${display_name}-${tag}" _myiot_archive_id)
    set(_myiot_download_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/downloads")
    set(_myiot_extract_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/extract-${_myiot_archive_id}")
    set(_myiot_archive_file "${_myiot_download_dir}/${_myiot_archive_id}.zip")
    file(MAKE_DIRECTORY "${_myiot_download_dir}")

    set(_myiot_attempt_index 1)
    foreach(_myiot_archive_url IN LISTS _myiot_archive_urls)
        file(APPEND "${_myiot_log_file}"
            "Attempt ${_myiot_attempt_index}: ${_myiot_archive_url}\n"
        )

        if(DEFINED MYIOT_BOOTSTRAP_SHOW_PROGRESS AND MYIOT_BOOTSTRAP_SHOW_PROGRESS)
            message(STATUS
                "[archive ${_myiot_attempt_index}/${_myiot_archive_attempt_count}] "
                "Downloading ${display_name} ${tag} from ${_myiot_archive_url}"
            )
            set(_myiot_download_progress_args SHOW_PROGRESS)
        else()
            unset(_myiot_download_progress_args)
        endif()

        file(REMOVE "${_myiot_archive_file}")
        file(REMOVE_RECURSE "${_myiot_extract_dir}")
        file(MAKE_DIRECTORY "${_myiot_extract_dir}")

        file(DOWNLOAD
            "${_myiot_archive_url}"
            "${_myiot_archive_file}"
            STATUS _myiot_download_status
            LOG _myiot_download_log
            TLS_VERIFY ON
            INACTIVITY_TIMEOUT 60
            TIMEOUT 600
            ${_myiot_download_progress_args}
        )

        list(GET _myiot_download_status 0 _myiot_download_code)
        list(GET _myiot_download_status 1 _myiot_download_message)

        file(APPEND "${_myiot_log_file}"
            "Download status: ${_myiot_download_code} (${_myiot_download_message})\n"
            "${_myiot_download_log}\n"
        )

        if(NOT _myiot_download_code EQUAL 0 OR NOT EXISTS "${_myiot_archive_file}")
            math(EXPR _myiot_attempt_index "${_myiot_attempt_index} + 1")
            continue()
        endif()

        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xf "${_myiot_archive_file}"
            WORKING_DIRECTORY "${_myiot_extract_dir}"
            RESULT_VARIABLE _myiot_extract_result
            OUTPUT_VARIABLE _myiot_extract_output
            ERROR_VARIABLE _myiot_extract_error
        )

        file(APPEND "${_myiot_log_file}"
            "Extract exit code: ${_myiot_extract_result}\n"
            "${_myiot_extract_output}\n"
            "${_myiot_extract_error}\n"
        )

        if(NOT _myiot_extract_result EQUAL 0)
            math(EXPR _myiot_attempt_index "${_myiot_attempt_index} + 1")
            continue()
        endif()

        file(GLOB _myiot_extracted_entries RELATIVE "${_myiot_extract_dir}" "${_myiot_extract_dir}/*")
        list(LENGTH _myiot_extracted_entries _myiot_extracted_count)

        if(NOT _myiot_extracted_count EQUAL 1)
            file(APPEND "${_myiot_log_file}"
                "Expected exactly one top-level entry in the extracted archive, got ${_myiot_extracted_count}.\n"
            )
            math(EXPR _myiot_attempt_index "${_myiot_attempt_index} + 1")
            continue()
        endif()

        list(GET _myiot_extracted_entries 0 _myiot_extracted_root_name)
        set(_myiot_extracted_root "${_myiot_extract_dir}/${_myiot_extracted_root_name}")

        if(NOT IS_DIRECTORY "${_myiot_extracted_root}" OR NOT EXISTS "${_myiot_extracted_root}/CMakeLists.txt")
            file(APPEND "${_myiot_log_file}"
                "The extracted archive does not look like a valid source tree: ${_myiot_extracted_root}\n"
            )
            math(EXPR _myiot_attempt_index "${_myiot_attempt_index} + 1")
            continue()
        endif()

        if(EXISTS "${source_dir}")
            myiot_bootstrap_remove_incomplete_source_tree("${source_dir}" "${display_name}")
        endif()

        file(RENAME "${_myiot_extracted_root}" "${source_dir}")
        file(APPEND "${_myiot_log_file}"
            "Archive fallback succeeded. Source tree created at: ${source_dir}\n"
        )
        file(REMOVE "${_myiot_archive_file}")
        file(REMOVE_RECURSE "${_myiot_extract_dir}")
        set(${success_var} TRUE PARENT_SCOPE)
        return()
    endforeach()

    file(REMOVE "${_myiot_archive_file}")
    file(REMOVE_RECURSE "${_myiot_extract_dir}")
endfunction()

function(myiot_bootstrap_clone_if_missing source_dir repository tag display_name)
    if(EXISTS "${source_dir}/CMakeLists.txt")
        message(STATUS "Using ${display_name} sources from: ${source_dir}")
        return()
    endif()

    myiot_bootstrap_require(MYIOT_BOOTSTRAP_DEPS_DIR)

    find_package(Git REQUIRED)
    file(MAKE_DIRECTORY "${MYIOT_BOOTSTRAP_DEPS_DIR}")

    if(EXISTS "${source_dir}")
        myiot_bootstrap_remove_incomplete_source_tree("${source_dir}" "${display_name}")
    endif()

    string(MAKE_C_IDENTIFIER "clone-${display_name}" _myiot_clone_log_name)
    set(_myiot_clone_base_command
        "${GIT_EXECUTABLE}"
        "-c" "advice.detachedHead=false"
        "-c" "http.version=HTTP/1.1"
        clone
        "--progress"
        "--single-branch"
        "--branch" "${tag}"
        "--depth" "1"
    )

    myiot_bootstrap_run_logged_result(
        _myiot_clone_result
        _myiot_clone_log_file
        "Cloning ${display_name} ${tag} into ${source_dir} with blob filtering"
        "${_myiot_clone_log_name}-partial"
        COMMAND
            ${_myiot_clone_base_command}
            "--filter=blob:none"
            "${repository}"
            "${source_dir}"
    )

    if(_myiot_clone_result EQUAL 0)
        return()
    endif()

    message(WARNING
        "Partial clone of ${display_name} failed with exit code ${_myiot_clone_result}. "
        "Retrying without --filter=blob:none because some Git for Windows HTTPS sessions fail "
        "while checkout fetches promisor objects. See log: ${_myiot_clone_log_file}"
    )

    if(EXISTS "${source_dir}")
        myiot_bootstrap_remove_incomplete_source_tree("${source_dir}" "${display_name}")
    endif()

    myiot_bootstrap_run_logged_result(
        _myiot_plain_clone_result
        _myiot_plain_clone_log_file
        "Retrying ${display_name} ${tag} clone into ${source_dir} without blob filtering"
        "${_myiot_clone_log_name}"
        COMMAND
            ${_myiot_clone_base_command}
            "${repository}"
            "${source_dir}"
    )

    if(_myiot_plain_clone_result EQUAL 0)
        return()
    endif()

    message(WARNING
        "Shallow clone of ${display_name} also failed with exit code ${_myiot_plain_clone_result}. "
        "Attempting a GitHub source archive download as a final fallback. "
        "See log: ${_myiot_plain_clone_log_file}"
    )

    if(EXISTS "${source_dir}")
        myiot_bootstrap_remove_incomplete_source_tree("${source_dir}" "${display_name}")
    endif()

    myiot_bootstrap_try_download_github_archive(
        _myiot_archive_success
        _myiot_archive_log_file
        "${source_dir}"
        "${repository}"
        "${tag}"
        "${display_name}"
        "${_myiot_clone_log_name}-archive"
    )

    if(_myiot_archive_success)
        return()
    endif()

    message(FATAL_ERROR
        "Downloading ${display_name} ${tag} into ${source_dir} failed after trying partial clone, "
        "shallow clone, and GitHub archive fallback. "
        "See logs: ${_myiot_clone_log_file}; ${_myiot_plain_clone_log_file}; ${_myiot_archive_log_file}"
    )
endfunction()

function(myiot_bootstrap_update_submodules source_dir display_name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs PATHS)
    cmake_parse_arguments(MYIOT_SUBMODULES "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT EXISTS "${source_dir}/.gitmodules")
        return()
    endif()

    if(NOT EXISTS "${source_dir}/.git")
        message(FATAL_ERROR
            "${display_name} source tree at ${source_dir} declares git submodules, "
            "but it is not a git checkout. Remove the directory so bootstrap can clone it again."
        )
    endif()

    find_package(Git REQUIRED)

    set(_myiot_submodule_command
        "${GIT_EXECUTABLE}"
        -C "${source_dir}"
        submodule
        update
        --init
        --recursive
    )

    if(MYIOT_SUBMODULES_PATHS)
        list(APPEND _myiot_submodule_command ${MYIOT_SUBMODULES_PATHS})
    endif()

    string(MAKE_C_IDENTIFIER "submodules-${display_name}" _myiot_submodule_log_name)
    myiot_bootstrap_run_logged(
        "Updating ${display_name} git submodules"
        "${_myiot_submodule_log_name}"
        COMMAND ${_myiot_submodule_command}
    )
endfunction()

function(myiot_bootstrap_append_common_configure_args out_var)
    set(_myiot_args ${ARGN})

    myiot_bootstrap_require(MYIOT_BOOTSTRAP_INSTALL_PREFIX)
    list(APPEND _myiot_args
        -DCMAKE_INSTALL_PREFIX:PATH=${MYIOT_BOOTSTRAP_INSTALL_PREFIX}
    )

    if(DEFINED MYIOT_BOOTSTRAP_PREFIX_PATH AND NOT MYIOT_BOOTSTRAP_PREFIX_PATH STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_PREFIX_PATH:PATH=${MYIOT_BOOTSTRAP_PREFIX_PATH}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_C_STANDARD AND NOT MYIOT_BOOTSTRAP_C_STANDARD STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_C_STANDARD:STRING=${MYIOT_BOOTSTRAP_C_STANDARD}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_C_FLAGS AND NOT MYIOT_BOOTSTRAP_C_FLAGS STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_C_FLAGS:STRING=${MYIOT_BOOTSTRAP_C_FLAGS}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_CXX_STANDARD AND NOT MYIOT_BOOTSTRAP_CXX_STANDARD STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_CXX_STANDARD:STRING=${MYIOT_BOOTSTRAP_CXX_STANDARD}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_CXX_FLAGS AND NOT MYIOT_BOOTSTRAP_CXX_FLAGS STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_CXX_FLAGS:STRING=${MYIOT_BOOTSTRAP_CXX_FLAGS}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_POSITION_INDEPENDENT_CODE AND NOT MYIOT_BOOTSTRAP_POSITION_INDEPENDENT_CODE STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=${MYIOT_BOOTSTRAP_POSITION_INDEPENDENT_CODE}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_C_COMPILER AND NOT MYIOT_BOOTSTRAP_C_COMPILER STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_C_COMPILER:FILEPATH=${MYIOT_BOOTSTRAP_C_COMPILER}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_C_COMPILER_ARG1 AND NOT MYIOT_BOOTSTRAP_C_COMPILER_ARG1 STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_C_COMPILER_ARG1:STRING=${MYIOT_BOOTSTRAP_C_COMPILER_ARG1}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_CXX_COMPILER AND NOT MYIOT_BOOTSTRAP_CXX_COMPILER STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_CXX_COMPILER:FILEPATH=${MYIOT_BOOTSTRAP_CXX_COMPILER}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_CXX_COMPILER_ARG1 AND NOT MYIOT_BOOTSTRAP_CXX_COMPILER_ARG1 STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_CXX_COMPILER_ARG1:STRING=${MYIOT_BOOTSTRAP_CXX_COMPILER_ARG1}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_RC_COMPILER AND NOT MYIOT_BOOTSTRAP_RC_COMPILER STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_RC_COMPILER:FILEPATH=${MYIOT_BOOTSTRAP_RC_COMPILER}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_RC_FLAGS AND NOT MYIOT_BOOTSTRAP_RC_FLAGS STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_RC_FLAGS:STRING=${MYIOT_BOOTSTRAP_RC_FLAGS}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_MT
       AND NOT MYIOT_BOOTSTRAP_MT STREQUAL ""
       AND NOT MYIOT_BOOTSTRAP_MT STREQUAL "CMAKE_MT-NOTFOUND")
        list(APPEND _myiot_args
            -DCMAKE_MT:FILEPATH=${MYIOT_BOOTSTRAP_MT}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_MAKE_PROGRAM AND NOT MYIOT_BOOTSTRAP_MAKE_PROGRAM STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_MAKE_PROGRAM:FILEPATH=${MYIOT_BOOTSTRAP_MAKE_PROGRAM}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_TOOLCHAIN_FILE AND NOT MYIOT_BOOTSTRAP_TOOLCHAIN_FILE STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${MYIOT_BOOTSTRAP_TOOLCHAIN_FILE}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_SYSROOT AND NOT MYIOT_BOOTSTRAP_SYSROOT STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_SYSROOT:PATH=${MYIOT_BOOTSTRAP_SYSROOT}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_BUILD_TYPE AND NOT MYIOT_BOOTSTRAP_BUILD_TYPE STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_BUILD_TYPE:STRING=${MYIOT_BOOTSTRAP_BUILD_TYPE}
        )
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_MSVC_RUNTIME_LIBRARY AND NOT MYIOT_BOOTSTRAP_MSVC_RUNTIME_LIBRARY STREQUAL "")
        list(APPEND _myiot_args
            -DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=${MYIOT_BOOTSTRAP_MSVC_RUNTIME_LIBRARY}
        )
    endif()

    set(${out_var} "${_myiot_args}" PARENT_SCOPE)
endfunction()

function(myiot_bootstrap_configure_build_install project_name project_source_dir binary_dir)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs EXTRA_CMAKE_ARGS)
    cmake_parse_arguments(MYIOT_BOOTSTRAP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    file(MAKE_DIRECTORY "${binary_dir}")

    set(_myiot_configure_command
        "${CMAKE_COMMAND}"
        -S "${project_source_dir}"
        -B "${binary_dir}"
    )
    if(DEFINED MYIOT_BOOTSTRAP_GENERATOR AND NOT MYIOT_BOOTSTRAP_GENERATOR STREQUAL "")
        list(APPEND _myiot_configure_command -G "${MYIOT_BOOTSTRAP_GENERATOR}")
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_GENERATOR_PLATFORM AND NOT MYIOT_BOOTSTRAP_GENERATOR_PLATFORM STREQUAL "")
        list(APPEND _myiot_configure_command -A "${MYIOT_BOOTSTRAP_GENERATOR_PLATFORM}")
    endif()
    if(DEFINED MYIOT_BOOTSTRAP_GENERATOR_TOOLSET AND NOT MYIOT_BOOTSTRAP_GENERATOR_TOOLSET STREQUAL "")
        list(APPEND _myiot_configure_command -T "${MYIOT_BOOTSTRAP_GENERATOR_TOOLSET}")
    endif()

    myiot_bootstrap_append_common_configure_args(
        _myiot_configure_args
        ${MYIOT_BOOTSTRAP_EXTRA_CMAKE_ARGS}
    )
    list(APPEND _myiot_configure_command ${_myiot_configure_args})

    set(_myiot_build_configs)
    if(DEFINED MYIOT_BOOTSTRAP_BUILD_CONFIGS AND NOT MYIOT_BOOTSTRAP_BUILD_CONFIGS STREQUAL "")
        string(REPLACE "|" ";" _myiot_build_configs "${MYIOT_BOOTSTRAP_BUILD_CONFIGS}")
    elseif(DEFINED MYIOT_BOOTSTRAP_MULTI_CONFIG AND MYIOT_BOOTSTRAP_MULTI_CONFIG)
        if(DEFINED MYIOT_BOOTSTRAP_DEBUG_CONFIG AND NOT MYIOT_BOOTSTRAP_DEBUG_CONFIG STREQUAL "")
            list(APPEND _myiot_build_configs "${MYIOT_BOOTSTRAP_DEBUG_CONFIG}")
        endif()
        if(DEFINED MYIOT_BOOTSTRAP_OPTIMIZED_CONFIG AND NOT MYIOT_BOOTSTRAP_OPTIMIZED_CONFIG STREQUAL "")
            list(APPEND _myiot_build_configs "${MYIOT_BOOTSTRAP_OPTIMIZED_CONFIG}")
        endif()
    endif()
    list(REMOVE_DUPLICATES _myiot_build_configs)

    if(_myiot_build_configs)
        list(LENGTH _myiot_build_configs _myiot_build_config_count)
        math(EXPR _myiot_total_steps "1 + (2 * ${_myiot_build_config_count})")
    else()
        set(_myiot_total_steps 3)
    endif()
    set(_myiot_step_index 1)

    string(MAKE_C_IDENTIFIER "${project_name}" _myiot_log_prefix)
    myiot_bootstrap_make_progress_description(
        _myiot_configure_description
        "${_myiot_step_index}"
        "${_myiot_total_steps}"
        "Configuring ${project_name}"
    )
    myiot_bootstrap_run_logged(
        "${_myiot_configure_description}"
        "${_myiot_log_prefix}-configure"
        COMMAND ${_myiot_configure_command}
    )
    math(EXPR _myiot_step_index "${_myiot_step_index} + 1")

    set(_myiot_parallel_args --parallel)
    if(DEFINED MYIOT_BOOTSTRAP_BUILD_PARALLEL_LEVEL
       AND NOT MYIOT_BOOTSTRAP_BUILD_PARALLEL_LEVEL STREQUAL "")
        list(APPEND _myiot_parallel_args "${MYIOT_BOOTSTRAP_BUILD_PARALLEL_LEVEL}")
    endif()

    if(_myiot_build_configs)
        foreach(_myiot_build_config IN LISTS _myiot_build_configs)
            string(TOLOWER "${_myiot_build_config}" _myiot_build_config_suffix)
            myiot_bootstrap_make_progress_description(
                _myiot_build_description
                "${_myiot_step_index}"
                "${_myiot_total_steps}"
                "Building ${project_name} (${_myiot_build_config})"
            )
            set(_myiot_build_command
                "${CMAKE_COMMAND}"
                --build "${binary_dir}"
                --config "${_myiot_build_config}"
                ${_myiot_parallel_args}
            )
            math(EXPR _myiot_install_step_index "${_myiot_step_index} + 1")
            myiot_bootstrap_make_progress_description(
                _myiot_install_description
                "${_myiot_install_step_index}"
                "${_myiot_total_steps}"
                "Installing ${project_name} (${_myiot_build_config}) to ${MYIOT_BOOTSTRAP_INSTALL_PREFIX}"
            )
            set(_myiot_install_command
                "${CMAKE_COMMAND}"
                --install "${binary_dir}"
                --config "${_myiot_build_config}"
            )

            myiot_bootstrap_run_logged(
                "${_myiot_build_description}"
                "${_myiot_log_prefix}-build-${_myiot_build_config_suffix}"
                COMMAND ${_myiot_build_command}
            )
            myiot_bootstrap_run_logged(
                "${_myiot_install_description}"
                "${_myiot_log_prefix}-install-${_myiot_build_config_suffix}"
                COMMAND ${_myiot_install_command}
            )
            math(EXPR _myiot_step_index "${_myiot_step_index} + 2")
        endforeach()
    else()
        myiot_bootstrap_make_progress_description(
            _myiot_build_description
            "${_myiot_step_index}"
            "${_myiot_total_steps}"
            "Building ${project_name}"
        )
        set(_myiot_build_command
            "${CMAKE_COMMAND}"
            --build "${binary_dir}"
            ${_myiot_parallel_args}
        )
        math(EXPR _myiot_install_step_index "${_myiot_step_index} + 1")
        myiot_bootstrap_make_progress_description(
            _myiot_install_description
            "${_myiot_install_step_index}"
            "${_myiot_total_steps}"
            "Installing ${project_name} to ${MYIOT_BOOTSTRAP_INSTALL_PREFIX}"
        )
        set(_myiot_install_command
            "${CMAKE_COMMAND}"
            --install "${binary_dir}"
        )

        myiot_bootstrap_run_logged(
            "${_myiot_build_description}"
            "${_myiot_log_prefix}-build"
            COMMAND ${_myiot_build_command}
        )
        myiot_bootstrap_run_logged(
            "${_myiot_install_description}"
            "${_myiot_log_prefix}-install"
            COMMAND ${_myiot_install_command}
        )
    endif()
endfunction()
