set(STOK_SERVICES_OUTPUT_DIR "${CMAKE_BINARY_DIR}/services")

function(stok_configure_service_target target_name output_name)
    set_target_properties(${target_name}
        PROPERTIES
            OUTPUT_NAME "${output_name}"
            RUNTIME_OUTPUT_DIRECTORY "${STOK_SERVICES_OUTPUT_DIR}"
    )

    if(CMAKE_CONFIGURATION_TYPES)
        foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
            string(TOUPPER "${config}" config_upper)
            set_target_properties(${target_name}
                PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${STOK_SERVICES_OUTPUT_DIR}"
            )
        endforeach()
    endif()

    myiot_copy_windows_runtime_dlls(${target_name})

    install(TARGETS ${target_name}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/services"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/services"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/services"
    )
endfunction()

function(stok_copy_service_config target_name source_name output_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/${source_name}"
            "$<TARGET_FILE_DIR:${target_name}>/${output_name}"
        VERBATIM
    )

    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/config/${source_name}"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/services"
        RENAME "${output_name}"
    )
endfunction()

add_library(stok_services_common STATIC
    common/include/stok/services/common/ServiceConfig.h
    common/include/stok/services/common/StockQuoteBus.h
    common/include/stok/services/common/TelemetryBootstrap.h
    common/src/ServiceConfig.cpp
    common/src/StockQuoteBus.cpp
    common/src/TelemetryBootstrap.cpp
)
set_target_properties(stok_services_common PROPERTIES DEBUG_POSTFIX d)

target_include_directories(stok_services_common
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/common/include"
)

target_link_libraries(stok_services_common
    PUBLIC
        Poco::Foundation
        Poco::JSON
        Poco::Net
        Poco::Util
        fastcdr
        fastdds
)

if(TARGET Poco::OpenTelemetry)
    target_link_libraries(stok_services_common PUBLIC Poco::OpenTelemetry)
endif()

if(STOK_ENABLE_MARKET_DATA_SERVICE)
    add_executable(stok_market_data_service
        market-data-service/src/main.cpp
    )

    target_link_libraries(stok_market_data_service
        PRIVATE
            stok_services_common
    )

    stok_configure_service_target(stok_market_data_service "stok-market-data-service")
    stok_copy_service_config(
        stok_market_data_service
        "market-data-service.properties"
        "market-data-service.properties"
    )
endif()

if(STOK_ENABLE_DESKTOP_SHELL)
    find_package(Qt6 QUIET COMPONENTS Core Gui Qml Quick QuickControls2)

    if(Qt6_FOUND)
        qt_standard_project_setup(REQUIRES 6.5)

        qt_add_executable(stok_desktop_shell
            WIN32
            desktop-shell/src/DesktopShellController.cpp
            desktop-shell/src/DesktopShellController.h
            desktop-shell/src/QuoteListModel.cpp
            desktop-shell/src/QuoteListModel.h
            desktop-shell/src/main.cpp
        )

        qt_add_qml_module(stok_desktop_shell
            URI StokDesktopShell
            VERSION 1.0
            QML_FILES
                desktop-shell/qml/Main.qml
        )

        target_link_libraries(stok_desktop_shell
            PRIVATE
                Qt6::Core
                Qt6::Gui
                Qt6::Qml
                Qt6::Quick
                Qt6::QuickControls2
                stok_services_common
        )

        stok_configure_service_target(stok_desktop_shell "stok-desktop-shell")
        stok_copy_service_config(
            stok_desktop_shell
            "desktop-shell.properties"
            "desktop-shell.properties"
        )

        if(WIN32)
            find_program(STOK_WINDEPLOYQT_EXECUTABLE
                NAMES windeployqt
                HINTS "${STOK_QT_ROOT}/bin"
            )

            if(STOK_WINDEPLOYQT_EXECUTABLE)
                add_custom_command(TARGET stok_desktop_shell POST_BUILD
                    COMMAND "${STOK_WINDEPLOYQT_EXECUTABLE}"
                        --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/desktop-shell/qml"
                        "$<TARGET_FILE:stok_desktop_shell>"
                    VERBATIM
                )
            else()
                message(WARNING
                    "windeployqt was not found. The desktop shell will build, but Qt runtime deployment "
                    "into the build tree is skipped."
                )
            endif()
        endif()
    else()
        message(WARNING
            "Qt6 was not found. Set STOK_QT_ROOT to your Qt 6 installation or disable "
            "STOK_ENABLE_DESKTOP_SHELL."
        )
    endif()
endif()
