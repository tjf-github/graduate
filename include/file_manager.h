#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
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

// 大文件上传会话
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

// 分块元数据
struct ChunkInfo
{
    std::string upload_id;
    int chunk_index;
    std::string chunk_hash;
    long long chunk_size;
    std::string status;
};

// 上传进度快照
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

// 文件管理器：封装文件存储与数据库元数据操作
class FileManager
{
public:
    FileManager(std::shared_ptr<DBConnectionPool> pool,
                const std::string &storage_path);

    // 大文件分块上传
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

    // 普通（非分块）上传
    std::optional<FileInfo> upload_file(int user_id,
                                        const std::string &original_filename,
                                        const std::string &mime_type,
                                        const char *data,
                                        size_t size);

    // 文件下载
    std::optional<std::vector<char>> download_file(int file_id, int user_id);

    // 获取文件信息
    std::optional<FileInfo> get_file_info(int file_id, int user_id);

    // 获取用户文件列表（分页）
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

    // 创建分享码
    std::string create_share_code(int file_id, int user_id);

    // 通过分享码获取文件信息
    std::optional<FileInfo> get_shared_file_info(const std::string &code);

    // 获取存储统计
    long long get_user_storage_used(int user_id);
    int get_user_file_count(int user_id);

    std::string generate_random_share_code(int length = 8);

private:
    // 数据库连接池
    std::shared_ptr<DBConnectionPool> db_pool;
    // 文件根目录
    std::string storage_path;
    // 兼容旧逻辑的预留字段
    std::string code;

    // 生成唯一文件名
    std::string generate_unique_filename(const std::string &original_filename);

    // 构建文件绝对路径：<storage>/<user_id>/<filename>
    std::string get_full_path(int user_id, const std::string &filename);

    // 创建用户目录
    bool create_user_directory(int user_id);

    // 分块上传临时路径
    std::string get_temp_dir(const std::string &upload_id);
    std::string get_chunk_path(const std::string &upload_id, int chunk_index);
    // 合并分块为最终文件
    bool merge_chunks(const std::string &upload_id,
                      int total_chunks,
                      const std::string &dest_path);
    std::string calculate_sha256(const char *data, size_t size);
};

#endif // FILE_MANAGER_H
