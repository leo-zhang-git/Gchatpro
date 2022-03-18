# CMakeList.txt: Gchatpro 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-std=c++11 -lpthread ")

project ("Gchatpro")

include_directories("include")

# 将源代码添加到此项目的可执行文件。
set(SRC_LIST "Gchatpro.cpp" "TcpServer.cpp" "jsoncpp.cpp")
add_executable (Gchatpro ${SRC_LIST})

# TODO: 如有需要，请添加测试并安装目标。
