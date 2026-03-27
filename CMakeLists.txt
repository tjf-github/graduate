##初始声明
cmake_minimum_required(VERSION 3.10)
project(CloudDisk)

##设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的库
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# MySQL连接器路径 (根据实际安装路径调整)
set(MYSQL_CONNECTOR_INCLUDE "/usr/include/mysql")
set(MYSQL_CONNECTOR_LIB "/usr/lib/x86_64-linux-gnu")

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${MYSQL_CONNECTOR_INCLUDE}
    ${OPENSSL_INCLUDE_DIR}
)

# 链接目录
link_directories(
    ${MYSQL_CONNECTOR_LIB}
)

# 源文件
set(SOURCES
    src/main.cpp
    src/logger.cpp
    src/server.cpp
    src/thread_pool.cpp
    src/database.cpp
    src/user_manager.cpp
    src/file_manager.cpp
    src/http_handler.cpp
    src/session_manager.cpp
    src/json_helper.cpp
)

# --- 修正后的片段 ---
# 生成可执行文件
add_executable(cloudisk_server ${SOURCES})

# 链接库 (删掉 uroot)
target_link_libraries(cloudisk_server
    Threads::Threads
    mysqlclient
    OpenSSL::SSL
    OpenSSL::Crypto
    uuid
    stdc++fs
)

# 编译选项
target_compile_options(cloudisk_server PRIVATE -Wall -Wextra -O2)

if(WIN32)
    # Windows 下可能需要手动指定路径或通过 VCPKG 查找
    # include_directories("C:/Path/To/MySQL/Include")
    # link_directories("C:/Path/To/MySQL/Lib")
    link_libraries(ws2_32 rpcrt4) # Winsock 和 UUID 库
else()
    # 原有的 Linux 配置...
endif()
