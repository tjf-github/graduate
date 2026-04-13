#include "server.h"
#include "logger.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

// 全局服务器实例指针，用于信号处理
CloudDiskServer *server_instance = nullptr;

// 捕捉SIGINT和SIGTERM信号，优雅关闭服务器
void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        LOG_INFO("Received shutdown signal...");
        if (server_instance)
        {
            server_instance->stop();
        }
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    // 默认配置
    int port = 8080;
    std::string storage_path = "./storage";
    std::string static_dir = "static";

    DBConfig db_config;
    db_config.host = "localhost";
    db_config.user = "tjf";
    db_config.password = "tjf927701";
    db_config.database = "cloudisk";
    db_config.port = 3306;

    // 从环境变量读取配置
    if (const char *env_port = std::getenv("SERVER_PORT"))
    {
        port = std::atoi(env_port);
    }

    if (const char *env_storage = std::getenv("STORAGE_PATH"))
    {
        storage_path = env_storage;
    }

    if (const char *env_db_host = std::getenv("DB_HOST"))
    {
        db_config.host = env_db_host;
    }

    if (const char *env_db_user = std::getenv("DB_USER"))
    {
        db_config.user = env_db_user;
    }

    if (const char *env_db_pass = std::getenv("DB_PASSWORD"))
    {
        db_config.password = env_db_pass;
    }

    if (const char *env_db_name = std::getenv("DB_NAME"))
    {
        db_config.database = env_db_name;
    }

    if (const char *env_db_port = std::getenv("DB_PORT"))
    {
        db_config.port = static_cast<unsigned int>(std::atoi(env_db_port));
    }

    if (const char *env_static_dir = std::getenv("STATIC_DIR"))
    {
        static_dir = env_static_dir;
    }

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "==================================" << std::endl;
    std::cout << "lightweight_comm_server v1.0" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Storage: " << storage_path << std::endl;
    std::cout << "Static Dir: " << static_dir << std::endl;
    std::cout << "Database: " << db_config.host << ":" << db_config.port
              << "/" << db_config.database << std::endl;
    std::cout << "==================================" << std::endl;

    // 创建服务器实例
    CloudDiskServer server(port, db_config, storage_path);
    // 将服务器实例指针赋值给全局变量，供信号处理使用
    server_instance = &server;

    // 启动服务器
    if (!server.start())
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server started successfully!" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    // 运行服务器
    server.run();

    return 0;
}
