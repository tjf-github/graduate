#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
// C++17引入的std::optional，用于表示可能不存在的值
#include <optional>
#include <ctime>
#include "database.h"

struct FileInfo
{
    int id;
    int user_id;
    std::string filename;
    std::string original_filename;
    std::string file_path;
    long long file_size;
    // MIME类型，例如"image/png"、"application/pdf"等，用于描述文件的格式和类型
    std::string mime_type;
    std::string upload_date;
};

struct ShareInfo
{
    std::string share_code;
    int owner_user_id;
    int file_id;
    std::string created_at;
};

// 上传会话结构体
struct UploadSession
{
    std::string upload_id;
    int user_id;
    std::string filename;
    long long total_size;
    int total_chunks;
    std::string mime_type;
    std::string created_at;
    std::string expires_at;
    std::string status;
};

// 分块信息结构体
struct ChunkInfo
{
    std::string upload_id;
    int chunk_index;
    std::string chunk_hash;
    long long chunk_size;
    std::string status;
};

// 上传进度结构体
struct UploadProgress
{
    std::string upload_id;
    int total_chunks;
    int completed_chunks;
    long long total_size;
    long long uploaded_size;
    double progress;
    std::vector<int> failed_chunks;
    std::string status;
    bool exists = false;
    bool expired = false;
};

// 文件管理类，需要对mysql和路径（有了路径就知道文件）进行封装，提供文件上传、下载、删除、重命名、搜索等功能
class FileManager
{
public:
    FileManager(std::shared_ptr<DBConnectionPool> pool,
                const std::string &storage_path);

    //  ── 大文件分块上传 ──────────────────────────────────
    std::optional<UploadSession> create_upload_session(
        int user_id,
        const std::string &filename,
        long long total_size,
        int total_chunks,
        const std::string &mime_type);

    bool save_chunk(
        const std::string &upload_id,
        int chunk_index,
        const std::string &expected_hash,
        const char *data,
        size_t size);

    UploadProgress get_upload_progress(const std::string &upload_id, int user_id);

    std::optional<FileInfo> complete_upload(
        const std::string &upload_id,
        int user_id);

    bool cancel_upload(const std::string &upload_id, int user_id);

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

    // 形成分享码
    std::string create_share_code(int file_id, int user_id);

    // 通过分享码获取文件信息
    std::optional<FileInfo> get_shared_file_info(const std::string &code);

    //  获取存储统计
    long long get_user_storage_used(int user_id);
    int get_user_file_count(int user_id);

    std::string generate_random_share_code(int length = 8);

private:
    // 数据库连接池
    std::shared_ptr<DBConnectionPool> db_pool;
    // 文件存储路径
    std::string storage_path;
    // 一个string作为分享码，生成分享码的逻辑可以是随机生成一个唯一的字符串，并将其与文件信息关联存储在数据库中，以便通过分享码查询文件信息和数据
    std::string code;

    // 生成唯一文件名
    std::string generate_unique_filename(const std::string &original_filename);

    // 构建完整文件路径，例如/storage/12345/unique_filename.ext
    std::string get_full_path(int user_id, const std::string &filename);

    // 创建用户目录
    bool create_user_directory(int user_id);

    // 临时目录路径
    std::string get_temp_dir(const std::string &upload_id);
    std::string get_chunk_path(const std::string &upload_id, int chunk_index);
    // 合并所有.part文件
    bool merge_chunks(const std::string &upload_id,
                      int total_chunks,
                      const std::string &dest_path);
    std::string calculate_sha256(const char *data, size_t size);
};

#endif // FILE_MANAGER_H
