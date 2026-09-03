cmake_minimum_required(VERSION 3.22)

# 服务端子项目（依赖隔离：构建目录 server/build，风格对齐 user/CMakeLists.txt）
# 第一阶段: 无界面控制台程序, 验证 数据库连接 + 数据访问层(DAO)
# 后续阶段: 在此基础上加 WebSockets 模块, 变身正式服务端
project(charge_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql WebSockets)

qt_add_executable(charge_server
    src/main.cpp
    src/Database.h
    src/Database.cpp
    src/UserDao.h
    src/UserDao.cpp
    src/StationDao.h
    src/StationDao.cpp
    src/AdminDao.h
    src/AdminDao.cpp
    src/ScreenDao.h
    src/ScreenDao.cpp
    src/WsServer.h
    src/WsServer.cpp
)

target_link_libraries(charge_server PRIVATE Qt6::Core Qt6::Sql Qt6::WebSockets)
