#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <string>
#include <memory>
#include <optional>
#include "database.h"

struct User {
    int id;
    std::string username;
    std::string email;
    std::string password_hash;
    long long storage_used;
    long long storage_limit;
    std::string created_at;
};

class UserManager {
public:
    explicit UserManager(std::shared_ptr<DBConnectionPool> pool);
    
    // 用户注册
    bool register_user(const std::string& username, 
                      const std::string& email, 
                      const std::string& password);
    
    // 用户登录
    std::optional<User> login(const std::string& username, 
                             const std::string& password);
    
    // 获取用户信息
    std::optional<User> get_user_by_id(int user_id);
    std::optional<User> get_user_by_username(const std::string& username);
    
    // 更新用户信息
    bool update_storage_used(int user_id, long long size_delta);
    bool update_password(int user_id, const std::string& new_password);
    bool update_profile(int user_id, const std::string& username, const std::string& email);
    
    // 检查用户存储空间
    bool check_storage_available(int user_id, long long file_size);
    
    // 删除用户
    bool delete_user(int user_id);
    
private:
    std::shared_ptr<DBConnectionPool> db_pool;
    
    // 密码哈希
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, 
                        const std::string& hash);
};

#endif // USER_MANAGER_H
