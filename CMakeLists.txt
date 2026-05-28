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
    src/network/debug_console_client.cpp
    src/ui/mainwindow.cpp
)

set(HEADERS
    src/network/debug_console_client.h
    src/ui/mainwindow.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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
