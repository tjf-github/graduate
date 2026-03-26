#include "user_manager.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <cstring>
#include "logger.h"

#include <iostream>

UserManager::UserManager(std::shared_ptr<DBConnectionPool> pool)
    : db_pool(pool) {}

// 初级hash，未加盐
std::string UserManager::hash_password(const std::string &password)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.length());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }
    // Debug: Print hash
    // std::cout << "Password: " << password << " -> Hash: " << ss.str() << std::endl;
    return ss.str();
}

bool UserManager::verify_password(const std::string &password,
                                  const std::string &hash)
{
    return hash_password(password) == hash;
}

bool UserManager::register_user(const std::string &username,
                                const std::string &email,
                                const std::string &password)
{
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    // 检查用户名是否已存在
    std::string check_query = "SELECT id FROM users WHERE username = '" +
                              db->escape_string(username) + "' OR email = '" +
                              db->escape_string(email) + "'";

    LOG_DEBUG("Register Query: " + check_query);
    MYSQL_RES *result = db->query(check_query);
    if (result)
    {
        int rows = mysql_num_rows(result);
        LOG_DEBUG("Check Rows: " + std::to_string(rows));
        mysql_free_result(result);
        // db_pool->return_connection(db);

        if (rows > 0)
        {
            return false; // 用户名或邮箱已存在
        }
    }

    // 插入新用户
    std::string password_hash = hash_password(password);
    std::string insert_query =
        "INSERT INTO users (username, email, password_hash, storage_used, storage_limit) VALUES ('" +
        db->escape_string(username) + "', '" +
        db->escape_string(email) + "', '" +
        password_hash + "', 0, 10737418240)"; // 10GB limit

    LOG_DEBUG("Insert Query: " + insert_query);
    bool success = db->execute(insert_query);
    if (success)
        LOG_DEBUG("Insert OK");
    else
        LOG_ERROR("Insert Failed");

    db_pool->return_connection(db);

    return success;
}

// 用户登录
std::optional<User> UserManager::login(const std::string &username,
                                       const std::string &password)
{
    auto db = db_pool->get_connection();
    if (!db)
        return std::nullopt;

    std::string query =
        "SELECT id, username, email, password_hash, storage_used, storage_limit, created_at "
        "FROM users WHERE username = '" +
        db->escape_string(username) + "'";

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row)
    {
        mysql_free_result(result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    std::string stored_hash = row[3] ? row[3] : "";

    // 验证密码
    if (!verify_password(password, stored_hash))
    {
        mysql_free_result(result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1] ? row[1] : "";
    user.email = row[2] ? row[2] : "";
    user.password_hash = stored_hash;
    user.storage_used = std::stoll(row[4] ? row[4] : "0");
    user.storage_limit = std::stoll(row[5] ? row[5] : "0");
    user.created_at = row[6] ? row[6] : "";

    mysql_free_result(result);
    db_pool->return_connection(db);

    return user;
}

std::optional<User> UserManager::get_user_by_id(int user_id)
{
    auto db = db_pool->get_connection();
    if (!db)
        return std::nullopt;

    std::string query =
        "SELECT id, username, email, password_hash, storage_used, storage_limit, created_at "
        "FROM users WHERE id = " +
        std::to_string(user_id);

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row)
    {
        mysql_free_result(result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1] ? row[1] : "";
    user.email = row[2] ? row[2] : "";
    user.password_hash = row[3] ? row[3] : "";
    user.storage_used = std::stoll(row[4] ? row[4] : "0");
    user.storage_limit = std::stoll(row[5] ? row[5] : "0");
    user.created_at = row[6] ? row[6] : "";

    mysql_free_result(result);
    db_pool->return_connection(db);

    return user;
}

std::optional<User> UserManager::get_user_by_username(const std::string &username)
{
    auto db = db_pool->get_connection();
    if (!db)
        return std::nullopt;

    std::string query =
        "SELECT id, username, email, password_hash, storage_used, storage_limit, created_at "
        "FROM users WHERE username = '" +
        db->escape_string(username) + "'";

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    // 通过看看是否有行来判断用户是否存在，如果没有行则返回nullopt
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row)
    {
        mysql_free_result(result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    User user;
    user.id = std::stoi(row[0]);
    user.username = row[1] ? row[1] : "";
    user.email = row[2] ? row[2] : "";
    user.password_hash = row[3] ? row[3] : "";
    user.storage_used = std::stoll(row[4] ? row[4] : "0");
    user.storage_limit = std::stoll(row[5] ? row[5] : "0");
    user.created_at = row[6] ? row[6] : "";

    mysql_free_result(result);
    db_pool->return_connection(db);

    return user;
}

bool UserManager::update_storage_used(int user_id, long long size_delta)
{
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    std::string query =
        "UPDATE users SET storage_used = storage_used + " +
        std::to_string(size_delta) + " WHERE id = " + std::to_string(user_id);

    bool success = db->execute(query);
    db_pool->return_connection(db);

    return success;
}

bool UserManager::check_storage_available(int user_id, long long file_size)
{
    auto user = get_user_by_id(user_id);
    if (!user)
        return false;

    return (user->storage_used + file_size) <= user->storage_limit;
}

bool UserManager::update_password(int user_id, const std::string &new_password)
{
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    std::string password_hash = hash_password(new_password);
    std::string query =
        "UPDATE users SET password_hash = '" + db->escape_string(password_hash) +
        "' WHERE id = " + std::to_string(user_id);

    bool success = db->execute(query);
    db_pool->return_connection(db);

    return success;
}

bool UserManager::update_profile(int user_id, const std::string &username, const std::string &email)
{
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    std::string escaped_username = db->escape_string(username);
    std::string escaped_email = db->escape_string(email);

    std::string duplicate_check =
        "SELECT id FROM users WHERE (username = '" + escaped_username +
        "' OR email = '" + escaped_email +
        "') AND id != " + std::to_string(user_id);

    MYSQL_RES *dup_result = db->query(duplicate_check);
    if (!dup_result)
    {
        db_pool->return_connection(db);
        return false;
    }

    int rows = mysql_num_rows(dup_result);
    mysql_free_result(dup_result);
    if (rows > 0)
    {
        db_pool->return_connection(db);
        return false;
    }

    std::string query =
        "UPDATE users SET username = '" + escaped_username +
        "', email = '" + escaped_email +
        "' WHERE id = " + std::to_string(user_id);

    bool success = db->execute(query);
    db_pool->return_connection(db);
    return success;
}

bool UserManager::delete_user(int user_id)
{
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    std::string query = "DELETE FROM users WHERE id = " + std::to_string(user_id);
    bool success = db->execute(query);
    db_pool->return_connection(db);

    return success;
}
