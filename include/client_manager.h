#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ClientInfo
{
    int socket_fd;
    std::string ip_address;
    int port;
    std::string connect_time;
    bool is_active;
};

class ClientManager
{
public:
    void add_client(int socket_fd, const std::string &ip, int port);
    void remove_client(int socket_fd);
    void mark_inactive(int socket_fd);
    bool is_connected(int socket_fd) const;

    std::optional<ClientInfo> get_client(int socket_fd) const;
    std::vector<ClientInfo> get_all_clients() const;
    size_t active_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, ClientInfo> clients_;
};

#endif // CLIENT_MANAGER_H
