#ifndef DATABASE_H
#define DATABASE_H
#include <condition_variable>

#ifdef _WIN32
#include <winsock2.h>
#include <mysql.h>
#else
// mysql/c++库
#include <mysql/mysql.h>
#endif

#include <string>
// 数据库配置结构体
#include <memory>
// 数据库连接类
#include <vector>
// 数据库连接池类
#include <mutex>

// 数据库配置结构体，包含连接数据库所需的参数
struct DBConfig
{
    std::string host = "localhost";
    std::string user = "tjf";
    std::string password = "tjf927701";
    std::string database = "cloudisk";
    unsigned int port = 3306;
};

// 数据库连接类，封装了MySQL连接和基本操作--连接、查询、预处理语句、事务等
class Database
{
public:
    explicit Database(const DBConfig &config);
    ~Database();

    // 连接管理
    bool connect();
    void disconnect();
    bool is_connected() const;

    // 执行查询
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
    std::string get_error() const;

private:
    // 连接对象
    MYSQL *conn;
    // 数据库配置
    DBConfig config;
    // 互斥锁，保护连接对象的线程安全
    mutable std::mutex db_mutex;
    // 连接状态
    bool connected;
};

// 数据库连接池
class DBConnectionPool
{
public:
    explicit DBConnectionPool(const DBConfig &config, size_t pool_size = 10);
    ~DBConnectionPool();

    std::shared_ptr<Database> get_connection();
    void return_connection(std::shared_ptr<Database> conn);

private:
    std::vector<std::shared_ptr<Database>> connections;
    std::vector<bool> available;
    DBConfig config;
    std::mutex pool_mutex;
    std::condition_variable cv;
};

#endif // DATABASE_H
