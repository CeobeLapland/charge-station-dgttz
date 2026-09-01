cmake_minimum_required(VERSION 3.22)

# 用户端子项目（依赖隔离：构建目录 user/build，模块引用隐式指向存在与否均可的 ../common）
project(charge_user_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 找到 Qt6 及用户端所需的模块
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 WebSockets Network
)

# 公共代码（协议常量/消息封装）位于仓库根的 common/，各端 include 而非复制。
# common 可能尚未就绪，仅加入 include 路径，不强制依赖其目标。
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common")
if(EXISTS "${COMMON_DIR}")
    include_directories("${COMMON_DIR}")
endif()

qt_add_executable(charge_user_client
    src/main.cpp
    src/UserClient.h
    src/UserClient.cpp
    src/AuthStore.h
    src/AuthStore.cpp
    src/Theme.h
    src/Theme.cpp
    qml/Main.qml
    qml/pages/LoginPage.qml
    qml/pages/RegisterPage.qml
    qml/pages/HomePage.qml
    qml/pages/OrderPage.qml
    qml/pages/ProfilePage.qml
    qml/components/BottomNav.qml
    qml/components/ConfirmDialog.qml
    qml/qtquickcontrols2.conf
)

qt_add_qml_module(charge_user_client
    URI UserClient
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/LoginPage.qml
        qml/pages/RegisterPage.qml
        qml/pages/HomePage.qml
        qml/pages/OrderPage.qml
        qml/pages/ProfilePage.qml
        qml/components/BottomNav.qml
        qml/components/ConfirmDialog.qml
    RESOURCES
        qml/qtquickcontrols2.conf
)

target_link_libraries(charge_user_client PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WebSockets
)

install(TARGETS charge_user_client
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)