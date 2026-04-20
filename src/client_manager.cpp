#include "client_manager.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

/**
 * @brief 匿名命名空间，包含内部辅助函数
 */
namespace
{
/**
 * @brief 获取当前时间的ISO 8601格式字符串
 * @return 格式为 "YYYY-MM-DDTHH:MM:SSZ" 的时间字符串（UTC时区）
 * 
 * 该函数用于记录客户端连接时间。时间格式遵循ISO 8601国际标准。
 * 实现中使用了平台特定的安全函数（Windows: gmtime_s, Unix: gmtime_r）
 * 来避免线程安全问题。
 */
std::string current_time_iso8601()
{
    // 获取当前系统时间点
    const auto now = std::chrono::system_clock::now();
    // 将chrono时间点转换为time_t（自纪元以来的秒数）
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_value;
    // 使用平台特定的函数将time_t转换为tm结构（避免gmtime()的线程安全问题）
#ifdef _WIN32
    gmtime_s(&tm_value, &now_time);  // Windows平台
#else
    gmtime_r(&now_time, &tm_value);  // Unix/Linux平台
#endif

    // 使用iomanip格式化输出为ISO 8601格式
    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
} // namespace

/**
 * @brief 添加新客户端到管理器
 * 
 * 创建客户端信息记录并加入管理器。连接时间自动设置为当前时间。
 * 所有新客户端初始状态为活跃。
 */
void ClientManager::add_client(int socket_fd, const std::string &ip, int port)
{
    // 使用RAII模式自动加锁和解锁，保证线程安全
    std::lock_guard<std::mutex> lock(mutex_);
    // 使用socket_fd作为key，创建新的客户端信息并存储
    clients_[socket_fd] = ClientInfo{socket_fd, ip, port, current_time_iso8601(), true};
}

/**
 * @brief 从管理器中移除客户端
 * 
 * 彻底删除客户端信息。如果客户端不存在则无操作。
 */
void ClientManager::remove_client(int socket_fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 从映射表中删除指定socket_fd对应的客户端
    clients_.erase(socket_fd);
}

/**
 * @brief 将客户端标记为非活跃状态
 * 
 * 用于在连接关闭或心跳超时时标记客户端。客户端仍保留在管理器中，
 * 但is_connected()会返回false。
 */
void ClientManager::mark_inactive(int socket_fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 查找指定的客户端
    auto it = clients_.find(socket_fd);
    // 如果找到则标记为非活跃
    if (it != clients_.end())
    {
        it->second.is_active = false;
    }
}

/**
 * @brief 检查客户端是否处于连接状态
 * @param socket_fd 要检查的客户端socket描述符
 * @return 如果客户端存在且处于活跃状态返回true，否则返回false
 */
bool ClientManager::is_connected(int socket_fd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 查找客户端并检查其活跃状态
    auto it = clients_.find(socket_fd);
    return it != clients_.end() && it->second.is_active;
}

/**
 * @brief 获取指定客户端的详细信息
 * @param socket_fd 要查询的客户端socket描述符
 * @return 如果客户端存在返回ClientInfo，否则返回nullopt
 */
std::optional<ClientInfo> ClientManager::get_client(int socket_fd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(socket_fd);
    // 客户端不存在则返回空值
    if (it == clients_.end())
    {
        return std::nullopt;
    }
    // 返回客户端信息的副本
    return it->second;
}

/**
 * @brief 获取所有客户端的信息
 * @return 包含所有客户端信息的向量
 * @note 返回的是当前时刻的快照，不会随后续修改而改变
 */
std::vector<ClientInfo> ClientManager::get_all_clients() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ClientInfo> result;
    // 预分配空间以避免多次重新分配
    result.reserve(clients_.size());
    // 遍历所有客户端，将其信息添加到结果向量
    for (const auto &entry : clients_)
    {
        result.push_back(entry.second);
    }
    return result;
}

/**
 * @brief 获取当前活跃客户端的数量
 * @return 活跃（is_active=true）的客户端数量
 */
size_t ClientManager::active_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    // 遍历所有客户端，统计活跃客户端的数量
    for (const auto &entry : clients_)
    {
        if (entry.second.is_active)
        {
            ++count;
        }
    }
    return count;
}
