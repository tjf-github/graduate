#include "file_manager.h"
#include <fstream>
#include <sstream>
#include <ctime>

#ifdef _WIN32
// Windows平台使用CoCreateGuid生成UUID，并使用direct.h来创建目录
// #include <direct.h>
// Windows平台使用objbase.h来生成UUID，并使用iomanip来格式化UUID字符串
// #include <objbase.h>
#include <iomanip>
// Windows implementation of uuid/uuid.h functionality using CoCreateGuid
#define mkdir(path, mode) _mkdir(path)
#else
// POSIX系统使用uuid/uuid.h
#include <uuid/uuid.h>
// POSIX系统使用sys/stat.h和sys/types.h来创建目录
#include <sys/stat.h>
// POSIX系统使用unistd.h来删除文件
#include <sys/types.h>
#include <unistd.h>
#endif
#include <random>

// 文件管理类实现
// 构造函数接受数据库连接池和存储路径，确保存储目录存在
FileManager::FileManager(std::shared_ptr<DBConnectionPool> pool,
                         const std::string &path)
    : db_pool(pool), storage_path(path)
{
    // 确保存储目录存在，调用mkdir创建目录--系统调用会根据路径自动创建多级目录，如果目录已存在则返回错误，但我们可以忽略这个错误
    mkdir(storage_path.c_str(), 0755);
}

// 生成唯一文件名，使用UUID并保留原始文件扩展名
std::string FileManager::generate_unique_filename(const std::string &original_filename)
{
    char uuid_str[37];

// 如果在Windows平台上，使用CoCreateGuid生成UUID，并格式化为字符串
#ifdef _WIN32
    GUID guid;
    if (CoCreateGuid(&guid) == S_OK)
    {
        snprintf(uuid_str, sizeof(uuid_str),
                 "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
                 guid.Data1, guid.Data2, guid.Data3,
                 guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                 guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
    else
    {
        // Fallback for failure
        return "";
    }

// 在POSIX系统上，使用uuid_generate生成UUID，并格式化为字符串
#else
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
#endif

    // 获取文件扩展名
    size_t dot_pos = original_filename.find_last_of('.');
    std::string extension = "";
    if (dot_pos != std::string::npos)
    {
        extension = original_filename.substr(dot_pos);
    }
    // 作为匿名对象拷贝返回，格式为UUID+扩展名
    return std::string(uuid_str) + extension;
}

// 构建完整文件路径，格式为storage_path/user_id/filename
std::string FileManager::get_full_path(int user_id, const std::string &filename)
{
    return storage_path + "/" + std::to_string(user_id) + "/" + filename;
}

// 创建用户目录，格式为storage_path/user_id，如果目录已存在则返回成功
bool FileManager::create_user_directory(int user_id)
{
    std::string user_dir = storage_path + "/" + std::to_string(user_id);
    // mkdir函数在成功创建目录时返回0，如果目录已存在则返回-1并设置errno为EEXIST，我们可以忽略这个错误
    return mkdir(user_dir.c_str(), 0755) == 0 || errno == EEXIST;
}

std::optional<UploadSession> create_upload_session(
    int user_id,
    const std::string &filename,
    long long total_size,
    int total_chunks,
    const std::string &mime_type)
{
}

// 文件上传，返回文件信息，如果上传失败则返回std::nullopt
// optional类是C++17引入的一个模板类，用于表示一个值可能存在也可能不存在的情况。它提供了一种安全的方式来处理可能为空的值，避免了使用裸指针或特殊值（如nullptr）来表示缺失数据的风险。
std::optional<FileInfo> FileManager::upload_file(int user_id,
                                                 const std::string &original_filename,
                                                 // mime_type是文件的MIME类型，例如"image/png"、"application/pdf"等，用于描述文件的格式和类型
                                                 const std::string &mime_type,
                                                 const char *data,
                                                 size_t size)
{
    // 创建用户目录
    if (!create_user_directory(user_id))
    {
        // 创建目录失败，可能是权限问题或磁盘错误，返回std::nullopt表示上传失败
        return std::nullopt;
    }

    // 生成唯一文件名
    std::string unique_filename = generate_unique_filename(original_filename);
    std::string full_path = get_full_path(user_id, unique_filename);

    // 写入文件
    std::ofstream file(full_path, std::ios::binary); // 打开
    if (!file)
    {
        return std::nullopt;
    }

    // 正式将数据写入文件，data是文件的二进制数据，size是文件的大小，单位为字节
    file.write(data, size);
    file.close();

    // 插入数据库记录
    auto db = db_pool->get_connection();
    // 如果获取数据库连接失败，删除已写入的文件并返回std::nullopt表示上传失败
    if (!db)
    {
        // 删除已写入的文件
        unlink(full_path.c_str());
        return std::nullopt;
    }

    // 构建SQL查询语句，使用escape_string函数对字符串进行转义，防止SQL注入攻击
    std::string query =
        "INSERT INTO files (user_id, filename, original_filename, file_path, file_size, mime_type) "
        "VALUES (" +
        std::to_string(user_id) + ", '" +
        db->escape_string(unique_filename) + "', '" +
        db->escape_string(original_filename) + "', '" +
        db->escape_string(full_path) + "', " +
        std::to_string(size) + ", '" +
        db->escape_string(mime_type) + "')";

    // 执行SQL查询，如果执行失败，删除已写入的文件并返回std::nullopt表示上传失败
    if (!db->execute(query))
    {
        db_pool->return_connection(db);
        unlink(full_path.c_str());
        return std::nullopt;
    }

    int file_id = db->last_insert_id();

    std::string update_storage_query =
        "UPDATE users SET storage_used = storage_used + " +
        std::to_string(size) + " WHERE id = " + std::to_string(user_id);

    if (!db->execute(update_storage_query))
    {
        std::string rollback_query =
            "DELETE FROM files WHERE id = " + std::to_string(file_id) +
            " AND user_id = " + std::to_string(user_id);
        db->execute(rollback_query);
        db_pool->return_connection(db);
        unlink(full_path.c_str());
        return std::nullopt;
    }

    db_pool->return_connection(db);

    // 构建返回信息
    FileInfo info;
    info.id = file_id;
    info.user_id = user_id;
    info.filename = unique_filename;
    info.original_filename = original_filename;
    info.file_path = full_path;
    info.file_size = size;
    info.mime_type = mime_type;

    return info;
}

// 文件下载，返回文件的二进制数据，如果文件不存在或用户没有权限访问则返回std::nullopt
std::optional<std::vector<char>> FileManager::download_file(int file_id, int user_id)
{
    // 获取文件信息
    auto file_info = get_file_info(file_id, user_id);
    if (!file_info)
    {
        return std::nullopt;
    }

    // 读取文件
    std::ifstream file(file_info->file_path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }

    // 将文件内容读取到一个vector<char>中，大小为file_info->file_size
    std::vector<char> data(file_info->file_size);
    file.read(data.data(), file_info->file_size);
    file.close();

    return data;
}

// 从mysql获取文件信息，如果文件不存在或用户没有权限访问则返回std::nullopt
std::optional<FileInfo> FileManager::get_file_info(int file_id, int user_id)
{
    auto db = db_pool->get_connection();
    if (!db)
        return std::nullopt;

    std::string query =
        "SELECT id, user_id, filename, original_filename, file_path, "
        "file_size, mime_type, upload_date "
        "FROM files WHERE id = " +
        std::to_string(file_id) +
        " AND user_id = " + std::to_string(user_id);

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    // 从结果集中获取第一行数据，如果没有数据则表示文件不存在或用户没有权限访问，返回std::nullopt
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row)
    {
        mysql_free_result(result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    FileInfo info;
    info.id = std::stoi(row[0]);
    info.user_id = std::stoi(row[1]);
    info.filename = row[2] ? row[2] : "";
    info.original_filename = row[3] ? row[3] : "";
    info.file_path = row[4] ? row[4] : "";
    info.file_size = std::stoll(row[5] ? row[5] : "0");
    info.mime_type = row[6] ? row[6] : "";
    info.upload_date = row[7] ? row[7] : "";

    mysql_free_result(result);
    db_pool->return_connection(db);

    return info;
}

// 获取用户所有文件列表，支持分页，返回一个FileInfo的vector，如果用户没有文件则返回一个空的vector
std::vector<FileInfo> FileManager::list_user_files(int user_id, int offset, int limit)
{
    std::vector<FileInfo> files;

    auto db = db_pool->get_connection();
    if (!db)
        return files;

    std::string query =
        "SELECT id, user_id, filename, original_filename, file_path, "
        "file_size, mime_type, upload_date "
        "FROM files WHERE user_id = " +
        std::to_string(user_id) +
        " ORDER BY upload_date DESC LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return files;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))
    {
        FileInfo info;
        info.id = std::stoi(row[0]);
        info.user_id = std::stoi(row[1]);
        info.filename = row[2] ? row[2] : "";
        info.original_filename = row[3] ? row[3] : "";
        info.file_path = row[4] ? row[4] : "";
        info.file_size = std::stoll(row[5] ? row[5] : "0");
        info.mime_type = row[6] ? row[6] : "";
        info.upload_date = row[7] ? row[7] : "";

        files.push_back(info);
    }

    mysql_free_result(result);
    db_pool->return_connection(db);

    return files;
}

// 删除文件，首先获取文件信息，如果文件不存在或用户没有权限访问则返回false，否则删除物理文件和数据库记录，返回true表示删除成功
bool FileManager::delete_file(int file_id, int user_id)
{
    // 获取文件信息
    auto file_info = get_file_info(file_id, user_id);
    if (!file_info)
    {
        return false;
    }

    // 删除物理文件
    unlink(file_info->file_path.c_str());

    // 删除数据库记录
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    std::string query =
        "DELETE FROM files WHERE id = " + std::to_string(file_id) +
        " AND user_id = " + std::to_string(user_id);

    bool success = db->execute(query);

    if (success)
    {
        std::string update_storage_query =
            "UPDATE users SET storage_used = GREATEST(0, storage_used - " +
            std::to_string(file_info->file_size) + ") WHERE id = " + std::to_string(user_id);
        success = db->execute(update_storage_query);
    }

    db_pool->return_connection(db);

    return success;
}

bool FileManager::rename_file(int file_id, int user_id,
                              const std::string &new_filename)
{
    auto db = db_pool->get_connection();
    if (!db)
        return false;

    std::string query =
        "UPDATE files SET original_filename = '" +
        db->escape_string(new_filename) +
        "' WHERE id = " + std::to_string(file_id) +
        " AND user_id = " + std::to_string(user_id);

    bool success = db->execute(query);
    db_pool->return_connection(db);

    return success;
}

// 搜索文件，返回匹配关键字的文件列表，支持分页，如果没有匹配的文件则返回一个空的vector
std::vector<FileInfo> FileManager::search_files(int user_id,
                                                const std::string &keyword)
{
    std::vector<FileInfo> files;

    auto db = db_pool->get_connection();
    if (!db)
        return files;

    std::string query =
        "SELECT id, user_id, filename, original_filename, file_path, "
        "file_size, mime_type, upload_date "
        "FROM files WHERE user_id = " +
        std::to_string(user_id) +
        " AND original_filename LIKE '%" + db->escape_string(keyword) + "%' "
                                                                        "ORDER BY upload_date DESC";

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return files;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))
    {
        FileInfo info;
        info.id = std::stoi(row[0]);
        info.user_id = std::stoi(row[1]);
        info.filename = row[2] ? row[2] : "";
        info.original_filename = row[3] ? row[3] : "";
        info.file_path = row[4] ? row[4] : "";
        info.file_size = std::stoll(row[5] ? row[5] : "0");
        info.mime_type = row[6] ? row[6] : "";
        info.upload_date = row[7] ? row[7] : "";

        files.push_back(info);
    }

    mysql_free_result(result);
    db_pool->return_connection(db);

    return files;
}

// 生成并保存分享码
std::string FileManager::create_share_code(int file_id, int user_id)
{
    std::string code = generate_random_share_code(8);
    auto db = db_pool->get_connection();
    if (!db)
        return "";

    std::string query =
        "UPDATE files SET share_code = '" + db->escape_string(code) +
        "' WHERE id = " + std::to_string(file_id) +
        " AND user_id = " + std::to_string(user_id);

    if (db->execute(query))
    {
        db_pool->return_connection(db);
        return code;
    }

    db_pool->return_connection(db);
    return "";
}

// 通过分享码下载文件
std::optional<FileInfo> FileManager::get_shared_file_info(const std::string &code)
{
    // 从数据库连接池获取一个可用连接
    auto db = db_pool->get_connection();
    if (!db)
        return std::nullopt;

    // 根据分享码查询文件信息，查询条件是 share_code 字段等于 code 参数
    // 返回文件的 id、user_id、filename、original_filename、file_path、file_size、mime_type 和 upload_date
    std::string query =
        "SELECT id, user_id, filename, original_filename, file_path, "
        "file_size, mime_type, upload_date "
        "FROM files WHERE share_code = '" +
        db->escape_string(code) + "'";

    // 执行 SQL 查询
    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    // 从结果集中获取第一行数据，如果没有数据则表示该分享码无效或已失效
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row)
    {
        mysql_free_result(result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    // 将数据库查询结果映射到 FileInfo 结构体中
    FileInfo info;
    info.id = std::stoi(row[0]);
    info.user_id = std::stoi(row[1]);
    info.filename = row[2] ? row[2] : "";
    info.original_filename = row[3] ? row[3] : "";
    info.file_path = row[4] ? row[4] : "";
    info.file_size = std::stoll(row[5] ? row[5] : "0");
    info.mime_type = row[6] ? row[6] : "";
    info.upload_date = row[7] ? row[7] : "";

    // 释放 MySQL 结果集并归还数据库连接到连接池
    mysql_free_result(result);
    db_pool->return_connection(db);

    return info;
}

// 获取存储统计，返回用户已使用的存储空间总大小和文件数量，如果用户没有文件则返回0
long long FileManager::get_user_storage_used(int user_id)
{
    auto db = db_pool->get_connection();
    if (!db)
        return 0;
    // 使用分享码查询文件信息，查询条件是share_code字段等于code参数，返回文件的id、user_id、filename、original_filename、file_path、file_size、mime_type和upload_date，如果没有找到匹配的记录则返回std::nullopt
    std::string query =
        "SELECT SUM(file_size) FROM files WHERE user_id = " +
        std::to_string(user_id);

    // 执行SQL查询，如果执行失败则返回0
    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return 0;
    }

    // 从结果集中获取第一行数据，如果没有数据则表示用户没有文件，返回0
    MYSQL_ROW row = mysql_fetch_row(result);
    long long total = 0;
    if (row && row[0])
    {
        total = std::stoll(row[0]);
    }

    //
    mysql_free_result(result);
    db_pool->return_connection(db);

    return total;
}

// 获取用户文件数量，如果用户没有文件则返回0
int FileManager::get_user_file_count(int user_id)
{
    auto db = db_pool->get_connection();
    if (!db)
        return 0;

    std::string query =
        "SELECT COUNT(*) FROM files WHERE user_id = " +
        std::to_string(user_id);

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = 0;
    if (row && row[0])
    {
        count = std::stoi(row[0]);
    }

    mysql_free_result(result);
    db_pool->return_connection(db);

    return count;
}

std::string FileManager::generate_random_share_code(int length)
{
    const std::string charset =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    // 使用 C++11 的随机数引擎
    std::random_device rd;  // 用于获取随机数种子
    std::mt19937 gen(rd()); // 标准 mersenne_twister_engine
    std::uniform_int_distribution<> dis(0, charset.length() - 1);

    std::string code;
    for (int i = 0; i < length; ++i)
    {
        code += charset[dis(gen)];
    }
    return code;
}
