# Bundles a self-contained Windows Python interpreter + AKShare into the
# build tree so end-users do not need to install Python separately to run
# the value-investment provider script.
#
# Output layout under the build directory:
#   <build>/python-embed/python.exe
#   <build>/python-embed/Lib/site-packages/akshare/...
#
# The install step copies the same tree into:
#   <prefix>/bin/python-embed/...
#   <prefix>/bin/tools/value_board_provider.py
#   <prefix>/bin/tools/free_data_provider.py
#
# market-board's controller searches for python-embed/python.exe relative to
# the running executable, so as long as this layout is preserved at install
# time the bundled interpreter is used automatically.

option(STOK_BUNDLE_PYTHON
    "Download embeddable Python and install AKShare into <build>/python-embed/"
    ON)

set(STOK_PYTHON_VERSION "3.11.9"
    CACHE STRING "Embeddable Python version to bundle")
set(STOK_PYTHON_EMBED_URL
    "https://www.python.org/ftp/python/${STOK_PYTHON_VERSION}/python-${STOK_PYTHON_VERSION}-embed-amd64.zip"
    CACHE STRING "URL for the embeddable Python zip")
set(STOK_GET_PIP_URL
    "https://bootstrap.pypa.io/get-pip.py"
    CACHE STRING "URL for pip's bootstrap script")
set(STOK_PYTHON_REQUIREMENTS
    "akshare"
    CACHE STRING "Python packages installed into the bundled interpreter")

if(NOT WIN32)
    message(STATUS "Stok Python bundle: only Windows is supported, skipping.")
    return()
endif()

if(NOT STOK_BUNDLE_PYTHON)
    message(STATUS "Stok Python bundle: STOK_BUNDLE_PYTHON=OFF, skipping. End-users must install Python + akshare manually.")
    return()
endif()

set(STOK_PYTHON_EMBED_ZIP "${CMAKE_BINARY_DIR}/python-embed-${STOK_PYTHON_VERSION}.zip")
set(STOK_PYTHON_EMBED_DIR "${CMAKE_BINARY_DIR}/python-embed")
set(STOK_PYTHON_EMBED_EXE "${STOK_PYTHON_EMBED_DIR}/python.exe")
set(STOK_PYTHON_BUNDLE_STAMP "${STOK_PYTHON_EMBED_DIR}/.stok-bundle.stamp")

# Step 1: download the embeddable zip if missing.
if(NOT EXISTS "${STOK_PYTHON_EMBED_ZIP}")
    message(STATUS "Downloading embeddable Python ${STOK_PYTHON_VERSION}...")
    file(DOWNLOAD
        "${STOK_PYTHON_EMBED_URL}"
        "${STOK_PYTHON_EMBED_ZIP}"
        SHOW_PROGRESS
        STATUS _stok_download_status
    )
    list(GET _stok_download_status 0 _stok_download_code)
    if(NOT _stok_download_code EQUAL 0)
        message(WARNING "Could not download embeddable Python: ${_stok_download_status}. Bundle disabled.")
        return()
    endif()
endif()

# Step 2: extract & enable site-packages, install pip + akshare. Stamp file
# avoids re-running this every reconfigure.
if(NOT EXISTS "${STOK_PYTHON_BUNDLE_STAMP}")
    file(REMOVE_RECURSE "${STOK_PYTHON_EMBED_DIR}")
    file(MAKE_DIRECTORY "${STOK_PYTHON_EMBED_DIR}")
    file(ARCHIVE_EXTRACT
        INPUT "${STOK_PYTHON_EMBED_ZIP}"
        DESTINATION "${STOK_PYTHON_EMBED_DIR}"
    )

    # Enable site-packages for the embeddable build. Without `import site`,
    # pip cannot find packages installed into Lib/site-packages.
    file(WRITE "${STOK_PYTHON_EMBED_DIR}/python311._pth"
"python311.zip
.
Lib/site-packages

# Site initialization is required by pip and akshare imports.
import site
")

    set(_stok_get_pip "${STOK_PYTHON_EMBED_DIR}/get-pip.py")
    if(NOT EXISTS "${_stok_get_pip}")
        message(STATUS "Downloading get-pip.py...")
        file(DOWNLOAD "${STOK_GET_PIP_URL}" "${_stok_get_pip}" SHOW_PROGRESS STATUS _stok_pip_status)
        list(GET _stok_pip_status 0 _stok_pip_code)
        if(NOT _stok_pip_code EQUAL 0)
            message(WARNING "Could not download get-pip.py: ${_stok_pip_status}. Bundle disabled.")
            return()
        endif()
    endif()

    message(STATUS "Bootstrapping pip into bundled Python...")
    execute_process(
        COMMAND "${STOK_PYTHON_EMBED_EXE}" "${_stok_get_pip}" --no-warn-script-location --no-cache-dir
        RESULT_VARIABLE _stok_pip_install_rc
        OUTPUT_VARIABLE _stok_pip_out
        ERROR_VARIABLE _stok_pip_err
    )
    if(NOT _stok_pip_install_rc EQUAL 0)
        message(WARNING "Could not bootstrap pip: ${_stok_pip_err}")
        return()
    endif()

    message(STATUS "Installing Python packages into bundled interpreter: ${STOK_PYTHON_REQUIREMENTS}")
    # Use a separate command so you can extend STOK_PYTHON_REQUIREMENTS via cache.
    set(_stok_pip_args "${STOK_PYTHON_EMBED_EXE}" -m pip install --no-warn-script-location --no-cache-dir)
    foreach(_pkg IN LISTS STOK_PYTHON_REQUIREMENTS)
        list(APPEND _stok_pip_args ${_pkg})
    endforeach()
    execute_process(
        COMMAND ${_stok_pip_args}
        RESULT_VARIABLE _stok_pkg_rc
        OUTPUT_VARIABLE _stok_pkg_out
        ERROR_VARIABLE _stok_pkg_err
    )
    if(NOT _stok_pkg_rc EQUAL 0)
        message(WARNING "Could not pip install ${STOK_PYTHON_REQUIREMENTS}: ${_stok_pkg_err}")
        return()
    endif()

    file(WRITE "${STOK_PYTHON_BUNDLE_STAMP}" "stok python bundle ready: ${STOK_PYTHON_VERSION} + ${STOK_PYTHON_REQUIREMENTS}\n")
    message(STATUS "Stok Python bundle ready at ${STOK_PYTHON_EMBED_DIR}")
endif()

# Step 3: install rules (run when `cmake --install` is invoked).
install(
    DIRECTORY "${STOK_PYTHON_EMBED_DIR}/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/python-embed"
    USE_SOURCE_PERMISSIONS
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "test"  EXCLUDE
)

# Install the data-provider scripts alongside the bundle so the controller's
# relative-path search resolves them at runtime. (Dev runs read directly from
# <repo>/tools/, but installed runs read from <prefix>/bin/tools/.)
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/tools/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/tools"
    FILES_MATCHING
    PATTERN "*.py"
    PATTERN "__pycache__" EXCLUDE
)
