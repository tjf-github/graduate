#include "server.h"
#include "http_handler.h"
#include "logger.h"
#include <iostream>
#include <cstring>
// 线程头文件
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <io.h>
#define close closesocket
#define usleep(x) Sleep((x) / 1000)
#else
#include <unistd.h>
// socket相关头文件
#include <sys/socket.h>
// 网络地址结构体
#include <netinet/in.h>
// IP地址转换函数
#include <arpa/inet.h>
#endif

CloudDiskServer::CloudDiskServer(int p, const DBConfig &db_config,
                                 const std::string &storage_path)
    : port(p), server_fd(-1), running(false)
{

    // 初始化线程池
    thread_pool = std::make_unique<ThreadPool>(10);

    // 初始化数据库连接池
    db_pool = std::make_shared<DBConnectionPool>(db_config, 10);

    // 初始化管理器
    user_manager = std::make_shared<UserManager>(db_pool);
    file_manager = std::make_shared<FileManager>(db_pool, storage_path);
    session_manager = std::make_shared<SessionManager>(30); // 30分钟超时

    // 初始化HTTP处理器
    http_handler = std::make_shared<HttpHandler>(
        user_manager, file_manager, session_manager);
}

CloudDiskServer::~CloudDiskServer()
{
    stop();
}

bool CloudDiskServer::create_socket()
{
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置socket选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0)
    {
        std::cerr << "setsockopt failed" << std::endl;
        close(server_fd);
        return false;
    }

    // 绑定地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        LOG_ERROR("Bind failed on port " + std::to_string(port));
        close(server_fd);
        return false;
    }

    // 开始监听
    if (listen(server_fd, 10) < 0)
    {
        LOG_ERROR("Listen failed");
        close(server_fd);
        return false;
    }

    LOG_INFO("Server listening on port " + std::to_string(port));
    return true;
}

bool CloudDiskServer::start()
{
    if (!create_socket())
    {
        return false;
    }

    running = true;

    // 启动会话清理线程
    std::thread cleanup_thread([this]()
                               { cleanup_sessions_periodically(); });
    cleanup_thread.detach();

    return true;
}

void CloudDiskServer::stop()
{
    running = false;

    if (server_fd != -1)
    {
        close(server_fd);
        server_fd = -1;
    }

    std::cout << "Server stopped" << std::endl;
}

// 处理客户端连接
void CloudDiskServer::handle_client(int client_fd)
{
    const int BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];

    // 读取请求
    std::string request_data;
    ssize_t bytes_read;

    while ((bytes_read = read(client_fd, buffer, BUFFER_SIZE)) > 0)
    {
        request_data.append(buffer, bytes_read);

        // 检查是否读取完整
        if (request_data.find("\r\n\r\n") != std::string::npos)
        {
            // 检查是否有Content-Length
            size_t cl_pos = request_data.find("Content-Length:");
            if (cl_pos != std::string::npos)
            {
                size_t cl_end = request_data.find("\r\n", cl_pos);
                std::string cl_str = request_data.substr(cl_pos + 15, cl_end - cl_pos - 15);
                int content_length = std::stoi(cl_str);

                size_t header_end = request_data.find("\r\n\r\n");
                int body_length = request_data.length() - header_end - 4;

                if (body_length < content_length)
                {
                    continue; // 继续读取
                }
            }
            break;
        }
    }

    if (bytes_read < 0)
    {
        std::cerr << "Error reading from client" << std::endl;
        close(client_fd);
        return;
    }

    // 解析请求
    HttpRequest request = HttpParser::parse(request_data);

    // 处理请求
    HttpResponse response = http_handler->handle_request(request);

    // 发送响应
    std::string response_str = HttpParser::build_response(response);
    write(client_fd, response_str.c_str(), response_str.length());

    // 关闭连接
    close(client_fd);
    std::cout << "Client disconnected" << std::endl;
}

// 服务器主循环
void CloudDiskServer::run()
{
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);

    while (running)
    {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_address,
                               &client_len);

        if (client_fd < 0)
        {
            if (running)
            {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }

        // 将客户端处理提交到线程池
        thread_pool->enqueue([this, client_fd]()
                             { this->handle_client(client_fd); });
    }
}

void CloudDiskServer::cleanup_sessions_periodically()
{
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        session_manager->cleanup_expired_sessions();
    }
}
