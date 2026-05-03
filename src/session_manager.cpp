#include "session_manager.h"
#include <random>
#include <sstream>
#include <iomanip>

SessionManager::SessionManager(int timeout_min)
    : timeout_minutes(timeout_min) {}

std::string SessionManager::generate_token()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned long long> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(16) << dis(gen)
       << std::setw(16) << dis(gen);

    return ss.str();
}

std::string SessionManager::create_session(int user_id)
{
    std::lock_guard<std::mutex> lock(session_mutex);

    std::string token = generate_token();

    Session session;
    session.token = token;
    session.user_id = user_id;
    session.created_at = std::chrono::system_clock::now();
    session.last_access = session.created_at;

    sessions[token] = session;

    return token;
}

bool SessionManager::is_expired(const Session &session)
{
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::minutes>(
        now - session.last_access);

    return diff.count() > timeout_minutes;
}

bool SessionManager::validate_session(const std::string &token)
{
    std::lock_guard<std::mutex> lock(session_mutex);

    auto it = sessions.find(token);
    if (it == sessions.end())
    {
        return false;
    }

    if (is_expired(it->second))
    {
        sessions.erase(it);
        return false;
    }

    // 更新最后访问时间
    it->second.last_access = std::chrono::system_clock::now();

    return true;
}

int SessionManager::get_user_id(const std::string &token)
{
    std::lock_guard<std::mutex> lock(session_mutex);

    auto it = sessions.find(token);
    if (it == sessions.end())
    {
        return -1;
    }

    if (is_expired(it->second))
    {
        sessions.erase(it);
        return -1;
    }

    return it->second.user_id;
}

bool SessionManager::delete_session(const std::string &token)
{
    std::lock_guard<std::mutex> lock(session_mutex);

    auto it = sessions.find(token);
    if (it == sessions.end())
    {
        return false;
    }

    sessions.erase(it);
    return true;
}

void SessionManager::cleanup_expired_sessions()
{
    std::lock_guard<std::mutex> lock(session_mutex);

    auto it = sessions.begin();
    while (it != sessions.end())
    {
        if (is_expired(it->second))
        {
            it = sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

size_t SessionManager::session_count()
{
    std::lock_guard<std::mutex> lock(session_mutex);
    return sessions.size();
}

size_t SessionManager::get_timeout_minutes() const
{
    return timeout_minutes;
}
