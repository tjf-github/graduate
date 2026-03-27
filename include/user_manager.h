#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <string>
#include <memory>
#include <optional>
#include "database.h"

struct User
{
    int id;
    std::string username;
    std::string email;
    std::string password_hash;
    // 用户存储空间使用情况，单位为字节
    long long storage_used;
    // 用户存储空间限制，单位为字节
    long long storage_limit;
    std::string created_at;
    // 发送和获取消息
    // std::string message;
    // int sender_id;
    // int receiver_id;
};

class UserManager
{
public:
    // 消息相关
    struct Message
    {
        int id;
        int sender_id;
        int receiver_id;
        std::string content;
        std::string created_at;
        bool is_read;
    };

    bool send_message(int sender_id, int receiver_id, const std::string &content);
    std::vector<Message> get_messages(int user_id, int with_user_id, int limit = 50);
    bool mark_messages_read(int user_id, int sender_id);
    explicit UserManager(std::shared_ptr<DBConnectionPool> pool);

    // 用户注册
    bool register_user(const std::string &username,
                       const std::string &email,
                       const std::string &password);

    // 用户登录
    std::optional<User> login(const std::string &username,
                              const std::string &password);

    // 获取用户信息
    std::optional<User> get_user_by_id(int user_id);
    std::optional<User> get_user_by_username(const std::string &username);

    // 更新用户信息
    bool update_storage_used(int user_id, long long size_delta);
    bool update_password(int user_id, const std::string &new_password);
    bool update_profile(int user_id, const std::string &username, const std::string &email);

    // 检查用户存储空间
    bool check_storage_available(int user_id, long long file_size);

    // 删除用户
    bool delete_user(int user_id);

private:
    // 数据库连接池
    std::shared_ptr<DBConnectionPool> db_pool;

    // 密码哈希
    std::string hash_password(const std::string &password);
    // 验证密码
    bool verify_password(const std::string &password,
                         const std::string &hash);
};

#endif // USER_MANAGER_H
