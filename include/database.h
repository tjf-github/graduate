#ifndef DATABASE_H
#define DATABASE_H
#include <condition_variable>

#ifdef _WIN32
#include <winsock2.h>
#include <mysql.h>
#else
// MySQL C API
#include <mysql/mysql.h>
#endif

#include <string>
// 数据库配置
#include <memory>
// 数据库连接封装
#include <vector>
// 数据库连接池
#include <mutex>

// 数据库连接参数
struct DBConfig
{
    std::string host = "localhost";
    std::string user = "tjf";
    std::string password = "tjf927701";
    std::string database = "cloudisk";
    unsigned int port = 3306;
};

// 单条数据库连接，封装 MySQL 连接与常用操作
class Database
{
public:
    explicit Database(const DBConfig &config);
    ~Database();

    // 连接生命周期
    bool connect();
    void disconnect();
    bool is_connected() const;

    // SQL 执行
    bool execute(const std::string &query);
    MYSQL_RES *query(const std::string &query);

    // 预处理语句
    MYSQL_STMT *prepare(const std::string &query);
    bool execute_prepared(MYSQL_STMT *stmt);

    // 事务
    bool begin_transaction();
    bool commit();
    bool rollback();

    // 工具函数
    std::string escape_string(const std::string &str);
    unsigned long long last_insert_id();
    unsigned long long affected_rows();
    std::string get_error() const;

private:
    // 原生 MySQL 连接句柄
    MYSQL *conn;
    // 连接配置
    DBConfig config;
    // 保护 conn 的并发访问
    mutable std::mutex db_mutex;
    // 当前连接状态
    bool connected;
};

// 简单阻塞式连接池：无可用连接时等待
class DBConnectionPool : public std::enable_shared_from_this<DBConnectionPool>
{
public:
    explicit DBConnectionPool(const DBConfig &config, size_t pool_size = 10);
    ~DBConnectionPool();

    std::shared_ptr<Database> get_connection();
    void return_connection(std::shared_ptr<Database> conn);

private:
    // 连接对象
    std::vector<std::shared_ptr<Database>> connections;
    // 连接占用状态，与 connections 下标一一对应
    std::vector<bool> available;
    DBConfig config;
    std::mutex pool_mutex;
    std::condition_variable cv;
};

#endif // DATABASE_H
