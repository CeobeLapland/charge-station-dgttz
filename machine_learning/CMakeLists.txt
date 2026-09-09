cmake_minimum_required(VERSION 3.22)
project(ml_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

# 静态库：ml_engine（不含 demo 主程序）
file(GLOB ML_ENGINE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER ML_ENGINE_SOURCES EXCLUDE REGEX ".*/ml_demo\\.cpp$")

add_library(ml_engine STATIC ${ML_ENGINE_SOURCES})
target_include_directories(ml_engine PUBLIC include)
target_link_libraries(ml_engine PUBLIC Qt6::Core Qt6::Sql)

# 独立自检 demo
qt_add_executable(ml_demo src/ml_demo.cpp)
target_link_libraries(ml_demo PRIVATE ml_engine)
