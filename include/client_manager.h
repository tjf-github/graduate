#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @struct ClientInfo
 * @brief 客户端信息结构体
 * 
 * 存储单个客户端连接的相关信息，包括套接字描述符、IP地址、端口、连接时间和活跃状态
 */
struct ClientInfo
{
    int socket_fd;           ///< 客户端套接字文件描述符
    std::string ip_address;  ///< 客户端IP地址
    int port;                ///< 客户端端口号
    std::string connect_time;///< 客户端连接时间（ISO 8601格式）
    bool is_active;          ///< 客户端是否处于活跃状态
};

/**
 * @class ClientManager
 * @brief 客户端连接管理器
 * 
 * 负责管理所有客户端连接，提供添加、删除、查询和统计客户端的功能。
 * 所有操作都是线程安全的，使用互斥锁保护共享数据。
 */
class ClientManager
{
public:
    /**
     * @brief 添加新客户端
     * @param socket_fd 客户端套接字文件描述符
     * @param ip 客户端IP地址
     * @param port 客户端端口号
     * @note 线程安全。连接时间自动设置为当前时间（ISO 8601格式）
     */
    void add_client(int socket_fd, const std::string &ip, int port);
    
    /**
     * @brief 移除指定的客户端
     * @param socket_fd 要移除的客户端套接字文件描述符
     * @note 线程安全。如果客户端不存在则无操作
     */
    void remove_client(int socket_fd);
    
    /**
     * @brief 将指定客户端标记为非活跃
     * @param socket_fd 客户端套接字文件描述符
     * @note 线程安全。常用于连接关闭时的清理操作
     */
    void mark_inactive(int socket_fd);
    
    /**
     * @brief 检查指定客户端是否处于连接状态
     * @param socket_fd 客户端套接字文件描述符
     * @return 如果客户端存在且活跃则返回true，否则返回false
     * @note 线程安全
     */
    bool is_connected(int socket_fd) const;

    /**
     * @brief 获取指定客户端的详细信息
     * @param socket_fd 客户端套接字文件描述符
     * @return 包含客户端信息的optional对象，如果客户端不存在则返回nullopt
     * @note 线程安全
     */
    std::optional<ClientInfo> get_client(int socket_fd) const;
    
    /**
     * @brief 获取所有客户端的信息
     * @return 包含所有客户端信息的向量
     * @note 线程安全。返回的是当前时刻的快照，不会随后续修改而改变
     */
    std::vector<ClientInfo> get_all_clients() const;
    
    /**
     * @brief 获取活跃客户端的数量
     * @return 当前活跃的客户端数量
     * @note 线程安全
     */
    size_t active_count() const;

private:
    mutable std::mutex mutex_;                          ///< 互斥锁，保护客户端映射表的并发访问
    std::unordered_map<int, ClientInfo> clients_;       ///< 以socket_fd为key的客户端信息映射表
};

#endif // CLIENT_MANAGER_H
