#ifndef SERVER_H
#define SERVER_H

#include <memory>
#include "thread_pool.h"
#include "database.h"
#include "user_manager.h"
#include "file_manager.h"
#include "http_handler.h"
#include "session_manager.h"

class CloudDiskServer {
public:
    CloudDiskServer(int port, const DBConfig& db_config, 
                   const std::string& storage_path);
    ~CloudDiskServer();
    
    // 启动服务器
    bool start();
    
    // 停止服务器
    void stop();
    
    // 运行服务器主循环
    void run();
    
private:
    int port;
    int server_fd;
    bool running;
    
    // 组件
    std::unique_ptr<ThreadPool> thread_pool;
    std::shared_ptr<DBConnectionPool> db_pool;
    std::shared_ptr<UserManager> user_manager;
    std::shared_ptr<FileManager> file_manager;
    std::shared_ptr<SessionManager> session_manager;
    std::shared_ptr<HttpHandler> http_handler;
    
    // 辅助函数
    bool create_socket();
    void handle_client(int client_fd);
    void cleanup_sessions_periodically();
};

#endif // SERVER_H
