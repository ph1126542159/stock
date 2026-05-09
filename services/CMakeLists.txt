set(STOK_SERVICES_OUTPUT_DIR "${CMAKE_BINARY_DIR}/services")

if(STOK_ENABLE_DESKTOP_SHELL)
    find_package(Qt6 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 Network Sql)
endif()

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
    get_filename_component(_stok_service_config_output_dir "${output_name}" DIRECTORY)
    if(_stok_service_config_output_dir)
        set(_stok_make_config_dir_command
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/${_stok_service_config_output_dir}"
        )
    else()
        set(_stok_make_config_dir_command)
    endif()

    add_custom_command(TARGET ${target_name} POST_BUILD
        ${_stok_make_config_dir_command}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/${source_name}"
            "$<TARGET_FILE_DIR:${target_name}>/${output_name}"
        VERBATIM
    )

    get_filename_component(_stok_service_config_install_dir "${output_name}" DIRECTORY)
    get_filename_component(_stok_service_config_install_name "${output_name}" NAME)
    if(_stok_service_config_install_dir)
        set(_stok_service_config_install_destination
            "${CMAKE_INSTALL_BINDIR}/services/${_stok_service_config_install_dir}")
    else()
        set(_stok_service_config_install_destination "${CMAKE_INSTALL_BINDIR}/services")
    endif()

    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/config/${source_name}"
        DESTINATION "${_stok_service_config_install_destination}"
        RENAME "${_stok_service_config_install_name}"
    )
endfunction()

function(stok_deploy_qml_runtime target_name qml_dir)
    if(WIN32 AND STOK_WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${STOK_WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${qml_dir}"
                "$<TARGET_FILE:${target_name}>"
            VERBATIM
        )
    endif()
endfunction()

add_library(stok_services_common STATIC
    common/include/stok/services/common/HostedViewPublisher.h
    common/include/stok/services/common/LocalizationClient.h
    common/include/stok/services/common/ServiceConfig.h
    common/include/stok/services/common/StockQuoteBus.h
    common/include/stok/services/common/TelemetryBootstrap.h
    common/include/stok/services/common/TextMessageBus.h
    common/src/HostedViewPublisher.cpp
    common/src/LocalizationClient.cpp
    common/src/ServiceConfig.cpp
    common/src/StockQuoteBus.cpp
    common/src/TelemetryBootstrap.cpp
    common/src/TextMessageBus.cpp
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

if(Qt6_FOUND)
    target_link_libraries(stok_services_common
        PUBLIC
            Qt6::Core
            Qt6::Gui
    )
    # LocalizationClient uses Q_OBJECT, so MOC must run on the static library.
    set_target_properties(stok_services_common PROPERTIES
        AUTOMOC ON
    )
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
        "runtime/market-data-service/config.properties"
    )
endif()

if(STOK_ENABLE_DESKTOP_SHELL)
    if(Qt6_FOUND)
        qt_standard_project_setup(REQUIRES 6.8)

        if(WIN32)
            find_program(STOK_WINDEPLOYQT_EXECUTABLE
                NAMES windeployqt
                HINTS "${STOK_QT_ROOT}/bin"
            )
        endif()

        qt_add_executable(stok_desktop_shell
            WIN32
            desktop-shell/src/DesktopShellController.cpp
            desktop-shell/src/DesktopShellController.h
            desktop-shell/src/ProcessMenuModel.cpp
            desktop-shell/src/ProcessMenuModel.h
            desktop-shell/src/QuoteListModel.cpp
            desktop-shell/src/QuoteListModel.h
            desktop-shell/src/LocalizationController.cpp
            desktop-shell/src/LocalizationController.h
            desktop-shell/src/main.cpp
        )
        qt_add_qml_module(stok_desktop_shell
            URI StokDesktopShell
            VERSION 1.0
            QML_FILES desktop-shell/qml/Main.qml
        )
        target_link_libraries(stok_desktop_shell PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network stok_services_common)
        stok_configure_service_target(stok_desktop_shell "stok-desktop-shell")
        stok_copy_service_config(stok_desktop_shell "desktop-shell.properties" "runtime/desktop-shell/config.properties")

        qt_add_executable(stok_market_board
            WIN32
            market-board/src/InstitutionRankingModel.cpp
            market-board/src/InstitutionRankingModel.h
            market-board/src/InvestmentOpportunityModel.cpp
            market-board/src/InvestmentOpportunityModel.h
            market-board/src/MarketBoardController.cpp
            market-board/src/MarketBoardController.h
            market-board/src/MarketBoardTypes.h
            market-board/src/UsMarketWatchModel.cpp
            market-board/src/UsMarketWatchModel.h
            market-board/src/main.cpp
        )
        qt_add_qml_module(stok_market_board
            URI StokMarketBoard
            VERSION 1.0
            QML_FILES market-board/qml/Main.qml
        )
        target_link_libraries(stok_market_board PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Sql stok_services_common)
        stok_configure_service_target(stok_market_board "stok-market-board")
        stok_copy_service_config(stok_market_board "market-board.properties" "runtime/market-board/config.properties")

        qt_add_executable(stok_config_center
            WIN32
            config-center/src/ConfigCenterController.cpp
            config-center/src/ConfigCenterController.h
            config-center/src/main.cpp
        )
        qt_add_qml_module(stok_config_center
            URI StokConfigCenter
            VERSION 1.0
            QML_FILES config-center/qml/Main.qml
        )
        target_link_libraries(stok_config_center PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 stok_services_common)
        stok_configure_service_target(stok_config_center "stok-config-center")
        stok_copy_service_config(stok_config_center "config-center.properties" "runtime/config-center/config.properties")

        qt_add_executable(stok_log_viewer
            WIN32
            log-viewer/src/LogProcessModel.cpp
            log-viewer/src/LogProcessModel.h
            log-viewer/src/LogViewerController.cpp
            log-viewer/src/LogViewerController.h
            log-viewer/src/main.cpp
        )
        qt_add_qml_module(stok_log_viewer
            URI StokLogViewer
            VERSION 1.0
            QML_FILES log-viewer/qml/Main.qml
        )
        target_link_libraries(stok_log_viewer PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 stok_services_common)
        stok_configure_service_target(stok_log_viewer "stok-log-viewer")
        stok_copy_service_config(stok_log_viewer "log-viewer.properties" "runtime/log-viewer/config.properties")

        qt_add_executable(stok_portfolio_board
            WIN32
            portfolio-board/src/HoldingsModel.cpp
            portfolio-board/src/HoldingsModel.h
            portfolio-board/src/PortfolioController.cpp
            portfolio-board/src/PortfolioController.h
            portfolio-board/src/PortfolioTypes.h
            portfolio-board/src/main.cpp
        )
        qt_add_qml_module(stok_portfolio_board
            URI StokPortfolioBoard
            VERSION 1.0
            QML_FILES portfolio-board/qml/Main.qml
        )
        target_link_libraries(stok_portfolio_board PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Sql stok_services_common)
        stok_configure_service_target(stok_portfolio_board "stok-portfolio-board")
        stok_copy_service_config(stok_portfolio_board "portfolio-board.properties" "runtime/portfolio-board/config.properties")

        function(stok_add_feature_page target_name output_name config_name default_id qml_uri qml_file main_source)
            qt_add_executable(${target_name}
                WIN32
                ${main_source}
                ${ARGN}
                feature-page/src/FeaturePageRunner.cpp
            )
            target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/feature-page/include")
            qt_add_qml_module(${target_name}
                URI ${qml_uri}
                VERSION 1.0
                QML_FILES ${qml_file}
            )
            target_link_libraries(${target_name} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 stok_services_common)
            stok_configure_service_target(${target_name} "${output_name}")
            stok_copy_service_config(${target_name} "${config_name}" "runtime/${default_id}/config.properties")
        endfunction()

        stok_add_feature_page(
            stok_valuation_research
            "stok-valuation-research"
            "valuation-research.properties"
            "valuation-research"
            "StokValuationResearch"
            "valuation-research/qml/Main.qml"
            "valuation-research/src/main.cpp"
            "valuation-research/src/ValuationResearchController.cpp"
            "valuation-research/src/ValuationResearchController.h"
        )

        stok_add_feature_page(
            stok_risk_backtest
            "stok-risk-backtest"
            "risk-backtest.properties"
            "risk-backtest"
            "StokRiskBacktest"
            "risk-backtest/qml/Main.qml"
            "risk-backtest/src/main.cpp"
            "risk-backtest/src/RiskBacktestController.cpp"
            "risk-backtest/src/RiskBacktestController.h"
        )

        stok_add_feature_page(
            stok_trade_alerts
            "stok-trade-alerts"
            "trade-alerts.properties"
            "trade-alerts"
            "StokTradeAlerts"
            "trade-alerts/qml/Main.qml"
            "trade-alerts/src/main.cpp"
            "trade-alerts/src/TradeAlertsController.cpp"
            "trade-alerts/src/TradeAlertsController.h"
        )

        stok_add_feature_page(
            stok_data_hub
            "stok-data-hub"
            "data-hub.properties"
            "data-hub"
            "StokDataHub"
            "data-hub/qml/Main.qml"
            "data-hub/src/main.cpp"
            "data-hub/src/DataHubController.cpp"
            "data-hub/src/DataHubController.h"
        )

        if(STOK_WINDEPLOYQT_EXECUTABLE)
            stok_deploy_qml_runtime(stok_desktop_shell "${CMAKE_CURRENT_SOURCE_DIR}/desktop-shell/qml")
            stok_deploy_qml_runtime(stok_market_board "${CMAKE_CURRENT_SOURCE_DIR}/market-board/qml")
            stok_deploy_qml_runtime(stok_config_center "${CMAKE_CURRENT_SOURCE_DIR}/config-center/qml")
            stok_deploy_qml_runtime(stok_log_viewer "${CMAKE_CURRENT_SOURCE_DIR}/log-viewer/qml")
            stok_deploy_qml_runtime(stok_portfolio_board "${CMAKE_CURRENT_SOURCE_DIR}/portfolio-board/qml")
            stok_deploy_qml_runtime(stok_valuation_research "${CMAKE_CURRENT_SOURCE_DIR}/valuation-research/qml")
            stok_deploy_qml_runtime(stok_risk_backtest "${CMAKE_CURRENT_SOURCE_DIR}/risk-backtest/qml")
            stok_deploy_qml_runtime(stok_trade_alerts "${CMAKE_CURRENT_SOURCE_DIR}/trade-alerts/qml")
            stok_deploy_qml_runtime(stok_data_hub "${CMAKE_CURRENT_SOURCE_DIR}/data-hub/qml")
        elseif(WIN32)
            message(WARNING
                "windeployqt was not found. The desktop shell will build, but Qt runtime deployment "
                "into the build tree is skipped."
            )
        endif()
    else()
        message(WARNING
            "Qt6 was not found. Set STOK_QT_ROOT to your Qt 6 installation or disable "
            "STOK_ENABLE_DESKTOP_SHELL."
        )
    endif()
endif()
