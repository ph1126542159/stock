myiot_add_osp_bundle(OSP_BundleAdmin_Bundle "com.appinf.osp.bundleadmin"
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleAdmin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleInfoHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleInstallHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleListHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RequestHandler.cpp
    LINK_LIBS Poco::OSPWeb Poco::OSP Poco::Net Poco::Util Poco::XML Poco::Foundation
    INCLUDE_DIRS
        ${MYIOT_OSP_BUNDLE_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    BUNDLE_SPEC ${CMAKE_CURRENT_SOURCE_DIR}/BundleAdmin.bndlspec
)
