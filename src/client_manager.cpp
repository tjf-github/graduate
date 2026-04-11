#include "client_manager.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace
{
std::string current_time_iso8601()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_value;
#ifdef _WIN32
    gmtime_s(&tm_value, &now_time);
#else
    gmtime_r(&now_time, &tm_value);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
} // namespace

void ClientManager::add_client(int socket_fd, const std::string &ip, int port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_[socket_fd] = ClientInfo{socket_fd, ip, port, current_time_iso8601(), true};
}

void ClientManager::remove_client(int socket_fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(socket_fd);
}

void ClientManager::mark_inactive(int socket_fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(socket_fd);
    if (it != clients_.end())
    {
        it->second.is_active = false;
    }
}

bool ClientManager::is_connected(int socket_fd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(socket_fd);
    return it != clients_.end() && it->second.is_active;
}

std::optional<ClientInfo> ClientManager::get_client(int socket_fd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(socket_fd);
    if (it == clients_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::vector<ClientInfo> ClientManager::get_all_clients() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ClientInfo> result;
    result.reserve(clients_.size());
    for (const auto &entry : clients_)
    {
        result.push_back(entry.second);
    }
    return result;
}

size_t ClientManager::active_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto &entry : clients_)
    {
        if (entry.second.is_active)
        {
            ++count;
        }
    }
    return count;
}
