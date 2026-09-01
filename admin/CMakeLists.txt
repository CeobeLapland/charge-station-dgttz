cmake_minimum_required(VERSION 3.16)

project(AdminClient VERSION 0.1.0 LANGUAGES CXX)

# ============================================================
# 依赖隔离：admin 只声明自己需要的 Qt 模块，不依赖根目录其他子项目。
# 同时支持 Qt6（Linux 正式构建，C++17）与 Qt5（Windows 预览，C++14）。
# 缺模块时：
#   Ubuntu: sudo apt install libqt6websockets6-dev libqt6charts6-dev
#   Windows: Qt MaintenanceTool 勾选 Charts / WebSockets 模块
# ============================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Widgets
    Charts
    Network
    WebSockets
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Widgets
    Charts
    Network
    WebSockets
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(QT_VERSION_MAJOR EQUAL 5)
    # Qt 5.8 自带 MinGW 5.3 不支持 C++17
    set(CMAKE_CXX_STANDARD 14)
endif()

add_executable(admin_client
    src/main.cpp
    resources/resources.qrc

    src/network/Protocol.h
    src/network/ServerConnection.h
    src/network/ServerConnection.cpp
    src/network/MockDataProvider.h
    src/network/MockDataProvider.cpp

    src/services/ApiClient.h
    src/services/ApiClient.cpp

    src/ui/LoginDialog.h
    src/ui/LoginDialog.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp

    src/ui/pages/SalesPage.h
    src/ui/pages/SalesPage.cpp
    src/ui/pages/ChargerStatusPage.h
    src/ui/pages/ChargerStatusPage.cpp
    src/ui/pages/ChargerManagePage.h
    src/ui/pages/ChargerManagePage.cpp
    src/ui/pages/StationManagePage.h
    src/ui/pages/StationManagePage.cpp
    src/ui/pages/UserManagePage.h
    src/ui/pages/UserManagePage.cpp
    src/ui/pages/DeviceOpsPage.h
    src/ui/pages/DeviceOpsPage.cpp
    src/ui/pages/DecisionPage.h
    src/ui/pages/DecisionPage.cpp
)

target_include_directories(admin_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(admin_client PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebSockets
)