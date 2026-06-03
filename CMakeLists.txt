cmake_minimum_required(VERSION 3.16)
project(CEPB_ControlCenter VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 优先，回退到 Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Widgets)

set(SOURCES
    src/main.cpp
    src/config/config_domain.cpp
    src/config/config_iec104_importer.cpp
    src/config/config_modbus_importer.cpp
    src/config/config_project_export.cpp
    src/config/modbus_mapping_utils.cpp
    src/config/config_project_manager.cpp
    src/network/debug_console_client.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow_debug.cpp
    src/ui/mainwindow_config.cpp
    src/ui/point_selector_dialog.cpp
)

set(HEADERS
    src/config/config_domain.h
    src/config/config_project_manager_p.h
    src/config/modbus_mapping_utils.h
    src/config/config_project_manager.h
    src/network/debug_console_client.h
    src/ui/mainwindow.h
    src/ui/point_selector_dialog.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
)

# 安装规则（交叉编译到嵌入式时有用）
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# CPack 发布打包配置
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "CEPB")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CEPB Control Center")
include(CPack)
