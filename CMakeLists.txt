# CMakeList.txt: Gchatpro 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

## 声明cmake版本
cmake_minimum_required (VERSION 3.8)

##  设置额外编译参数
set(CMAKE_CXX_STANDARD 17)
add_definitions(-std=c++11)
add_definitions(-DUNICODE -D_UNICODE)

##  项目名
project ("Gchatpro")

##  包含头文件目录
include_directories("include" "/www/server/mysql/include" )

##  连接库目录
link_directories("/www/server/mysql/lib/")

# 将源代码添加到此项目的可执行文件。
set(SRC_LIST "Gchatpro.cpp" "TcpServer.cpp" "jsoncpp.cpp" "threadpool.cpp" "DBC.cpp")
add_executable (Gchatpro ${SRC_LIST})

##  将库连接到可执行文件
target_link_libraries(Gchatpro "mysqlclient" "pthread")

