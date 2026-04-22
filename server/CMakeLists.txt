add_executable(macchina
    src/MacchinaServer.cpp
)

target_link_libraries(macchina
    PRIVATE
        Poco::OSP
        Poco::Zip
        Poco::Util
        Poco::JSON
        Poco::XML
        Poco::Foundation
)

if(TARGET Poco::OpenTelemetry)
    target_link_libraries(macchina PRIVATE Poco::OpenTelemetry)
endif()

add_custom_command(TARGET macchina POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/macchina.properties"
        "$<TARGET_FILE_DIR:macchina>/macchina.properties"
    VERBATIM
)

myiot_copy_windows_runtime_dlls(macchina)

if(WIN32 AND MYIOT_OPENSSL_RUNTIME_DLLS)
    foreach(openssl_dll IN LISTS MYIOT_OPENSSL_RUNTIME_DLLS)
        if(EXISTS "${openssl_dll}")
            add_custom_command(TARGET macchina POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${openssl_dll}"
                    "$<TARGET_FILE_DIR:macchina>/"
                VERBATIM
            )
        endif()
    endforeach()
endif()

if(WIN32 AND EXISTS "${MYIOT_OPENSSL_LEGACY_PROVIDER}")
    add_custom_command(TARGET macchina POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYIOT_OPENSSL_LEGACY_PROVIDER}"
            "$<TARGET_FILE_DIR:macchina>/legacy.dll"
        VERBATIM
    )
endif()

if(WIN32 AND EXISTS "${MYIOT_OPENSSL_CONFIG_FILE}")
    add_custom_command(TARGET macchina POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:macchina>/cnf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYIOT_OPENSSL_CONFIG_FILE}"
            "$<TARGET_FILE_DIR:macchina>/cnf/openssl.cnf"
        VERBATIM
    )
endif()

install(TARGETS macchina
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/macchina.properties.install"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    RENAME macchina.properties
)

if(WIN32 AND MYIOT_OPENSSL_RUNTIME_DLLS)
    foreach(openssl_dll IN LISTS MYIOT_OPENSSL_RUNTIME_DLLS)
        if(EXISTS "${openssl_dll}")
            install(FILES
                "${openssl_dll}"
                DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
        endif()
    endforeach()
endif()

if(WIN32 AND EXISTS "${MYIOT_OPENSSL_LEGACY_PROVIDER}")
    install(FILES
        "${MYIOT_OPENSSL_LEGACY_PROVIDER}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(WIN32 AND EXISTS "${MYIOT_OPENSSL_CONFIG_FILE}")
    install(FILES
        "${MYIOT_OPENSSL_CONFIG_FILE}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/cnf
    )
endif()
