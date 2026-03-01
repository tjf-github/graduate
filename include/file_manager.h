#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
// C++17引入的std::optional，用于表示可能不存在的值
#include <optional>
#include "database.h"

struct FileInfo
{
    int id;
    int user_id;
    std::string filename;
    std::string original_filename;
    std::string file_path;
    long long file_size;
    std::string mime_type;
    std::string upload_date;
};

// 文件管理类，需要对mysql和路径（有了路径就知道文件）进行封装，提供文件上传、下载、删除、重命名、搜索等功能
class FileManager
{
public:
    FileManager(std::shared_ptr<DBConnectionPool> pool,
                const std::string &storage_path);

    // 文件上传
    std::optional<FileInfo> upload_file(int user_id,
                                        const std::string &original_filename,
                                        // mime_type是文件的MIME类型，例如"image/png"、"application/pdf"等，用于描述文件的格式和类型
                                        const std::string &mime_type,
                                        // data是文件的二进制数据，size是文件的大小，单位为字节
                                        const char *data,
                                        size_t size);

    // 文件下载
    std::optional<std::vector<char>> download_file(int file_id, int user_id);

    // 获取文件信息
    std::optional<FileInfo> get_file_info(int file_id, int user_id);

    // 获取用户所有文件列表
    std::vector<FileInfo> list_user_files(int user_id,
                                          int offset = 0,
                                          int limit = 100);

    // 删除文件
    bool delete_file(int file_id, int user_id);

    // 重命名文件
    bool rename_file(int file_id, int user_id,
                     const std::string &new_filename);

    // 搜索文件
    std::vector<FileInfo> search_files(int user_id,
                                       const std::string &keyword);

    // 获取存储统计
    long long get_user_storage_used(int user_id);
    int get_user_file_count(int user_id);

private:
    // 数据库连接池
    std::shared_ptr<DBConnectionPool> db_pool;
    // 文件存储路径
    std::string storage_path;

    // 生成唯一文件名
    std::string generate_unique_filename(const std::string &original_filename);

    // 构建完整文件路径
    std::string get_full_path(int user_id, const std::string &filename);

    // 创建用户目录
    bool create_user_directory(int user_id);
};

#endif // FILE_MANAGER_H
