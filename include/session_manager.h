#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <string>
#include <map>
#include <memory>
#include <mutex>
// 处理日期头文件
#include <chrono>

struct Session
{
    std::string token;
    int user_id;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_access;
};

class SessionManager
{
public:
    SessionManager(int timeout_minutes = 30);

    // 创建会话
    std::string create_session(int user_id);

    // 验证会话
    bool validate_session(const std::string &token);

    // 获取用户ID
    int get_user_id(const std::string &token);

    // 删除会话
    bool delete_session(const std::string &token);

    // 清理过期会话
    void cleanup_expired_sessions();

    // 当前会话数量
    size_t session_count();

    // 获取会话超时时间
    size_t get_timeout_minutes() const;

private:
    // 存储会话的映射，key为token，value为Session对象
    std::map<std::string, Session> sessions;
    // 保护会话数据的互斥锁
    mutable std::mutex session_mutex;
    // 会话过期时间（分钟）
    int timeout_minutes;

    // 生成随机token
    std::string generate_token();

    // 检查会话是否过期
    bool is_expired(const Session &session);
};

#endif // SESSION_MANAGER_H
