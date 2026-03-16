#include "database.h"
#include "logger.h"
#include <iostream>
#include <cstring>

Database::Database(const DBConfig &cfg)
    : conn(nullptr), config(cfg), connected(false)
{
    conn = mysql_init(nullptr);
    if (!conn)
    {
        // 抛出异常，表示MySQL初始化失败
        throw std::runtime_error("MySQL initialization failed");
    }
}

Database::~Database()
{
    disconnect();
    if (conn)
    {
        mysql_close(conn);
    }
}

bool Database::connect()
{
    std::lock_guard<std::mutex> lock(db_mutex);

    // 设置字符集
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // 连接数据库
    if (!mysql_real_connect(conn,
                            config.host.c_str(),
                            config.user.c_str(),
                            config.password.c_str(),
                            config.database.c_str(),
                            config.port,
                            nullptr, 0))
    {
        std::cerr << "MySQL connection failed: " << mysql_error(conn) << std::endl;
        connected = false;
        return false;
    }

    connected = true;
    std::cout << "MySQL connected successfully" << std::endl;
    return true;
}

void Database::disconnect()
{
    std::lock_guard<std::mutex> lock(db_mutex);
    if (connected && conn)
    {
        mysql_close(conn);
        connected = false;
    }
}

bool Database::is_connected() const
{
    return connected;
}

// 执行SQL查询，返回是否成功
bool Database::execute(const std::string &query)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    if (!connected)
    {
        std::cerr << "Database not connected" << std::endl;
        return false;
    }

    if (mysql_query(conn, query.c_str()))
    {
        std::cerr << "Query failed: " << mysql_error(conn) << std::endl;
        return false;
    }

    return true;
}

// 执行SQL查询，返回结果集
MYSQL_RES *Database::query(const std::string &query)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    if (!connected)
    {
        std::cerr << "Database not connected" << std::endl;
        return nullptr;
    }

    if (mysql_query(conn, query.c_str()))
    {
        std::cerr << "Query failed: " << mysql_error(conn) << std::endl;
        return nullptr;
    }

    return mysql_store_result(conn);
}

// 预处理SQL语句，返回预处理对象
MYSQL_STMT *Database::prepare(const std::string &query)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    if (!connected)
    {
        return nullptr;
    }

    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        return nullptr;
    }

    if (mysql_stmt_prepare(stmt, query.c_str(), query.length()))
    {
        mysql_stmt_close(stmt);
        return nullptr;
    }

    return stmt;
}

// 执行预处理语句，返回是否成功
bool Database::execute_prepared(MYSQL_STMT *stmt)
{
    if (!stmt)
        return false;
    return mysql_stmt_execute(stmt) == 0;
}

// 事务管理
bool Database::begin_transaction()
{
    return execute("START TRANSACTION");
}

// 提交事务
bool Database::commit()
{
    return execute("COMMIT");
}

// 回滚事务
bool Database::rollback()
{
    return execute("ROLLBACK");
}

// 转义字符串，防止SQL注入攻击
std::string Database::escape_string(const std::string &str)
{
    std::lock_guard<std::mutex> lock(db_mutex);

    char *escaped = new char[str.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped, str.c_str(), str.length());
    std::string result(escaped);
    delete[] escaped;
    return result;
}

// 获取最后插入的ID
unsigned long long Database::last_insert_id()
{
    std::lock_guard<std::mutex> lock(db_mutex);
    return mysql_insert_id(conn);
}

// 获取错误信息
std::string Database::get_error() const
{
    std::lock_guard<std::mutex> lock(db_mutex);
    return std::string(mysql_error(conn));
}

// 连接池实现
DBConnectionPool::DBConnectionPool(const DBConfig &cfg, size_t pool_size)
    : config(cfg)
{
    for (size_t i = 0; i < pool_size; ++i)
    {
        auto db = std::make_shared<Database>(config);
        if (db->connect())
        {
            connections.push_back(db);
            available.push_back(true);
        }
    }
}

// 析构函数，清理连接池中的连接
DBConnectionPool::~DBConnectionPool()
{
    connections.clear();
}

// 获取一个可用的数据库连接，如果没有可用连接则等待
std::shared_ptr<Database> DBConnectionPool::get_connection()
{
    std::unique_lock<std::mutex> lock(pool_mutex);

    cv.wait(lock, [this]
            {
        for(size_t i = 0; i < available.size(); ++i) {
            if(available[i]) return true;
        }
        return false; });

    for (size_t i = 0; i < available.size(); ++i)
    {
        if (available[i])
        {
            available[i] = false;
            return connections[i];
        }
    }

    return nullptr;
}

// 归还连接到连接池
void DBConnectionPool::return_connection(std::shared_ptr<Database> conn)
{
    std::lock_guard<std::mutex> lock(pool_mutex);

    for (size_t i = 0; i < connections.size(); ++i)
    {
        if (connections[i] == conn)
        {
            available[i] = true;
            cv.notify_one();
            break;
        }
    }
}
