#include "file_manager.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cerrno>
#include <filesystem>
#include <iomanip>
#include <openssl/sha.h>

#ifdef _WIN32
#define mkdir(path, mode) _mkdir(path)
#else
#include <uuid/uuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <random>

namespace fs = std::filesystem;

FileManager::FileManager(std::shared_ptr<DBConnectionPool> pool,
                         const std::string &path)
    : db_pool(pool), storage_path(path)
{
    mkdir(storage_path.c_str(), 0755);
    fs::create_directories(storage_path + "/uploads");
}

std::string FileManager::generate_unique_filename(const std::string &original_filename)
{
    char uuid_str[37];

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
        return "";
    }
#else
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
#endif

    size_t dot_pos = original_filename.find_last_of('.');
    std::string extension = "";
    if (dot_pos != std::string::npos)
    {
        extension = original_filename.substr(dot_pos);
    }

    return std::string(uuid_str) + extension;
}

std::string FileManager::get_full_path(int user_id, const std::string &filename)
{
    return storage_path + "/" + std::to_string(user_id) + "/" + filename;
}

bool FileManager::create_user_directory(int user_id)
{
    std::string user_dir = storage_path + "/" + std::to_string(user_id);
    return mkdir(user_dir.c_str(), 0755) == 0 || errno == EEXIST;
}

std::string FileManager::get_temp_dir(const std::string &upload_id)
{
    return storage_path + "/uploads/" + upload_id;
}

std::string FileManager::get_chunk_path(const std::string &upload_id, int chunk_index)
{
    return get_temp_dir(upload_id) + "/chunk_" + std::to_string(chunk_index);
}

std::string FileManager::calculate_sha256(const char *data, size_t size)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(data), size, hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::optional<UploadSession> FileManager::create_upload_session(
    int user_id,
    const std::string &filename,
    long long total_size,
    int total_chunks,
    const std::string &mime_type)
{
    if (user_id <= 0 || filename.empty() || total_size <= 0 || total_chunks <= 0)
    {
        return std::nullopt;
    }

    auto db = db_pool->get_connection();
    if (!db)
    {
        return std::nullopt;
    }

    std::string quota_query =
        "SELECT storage_used, storage_limit FROM users WHERE id = " + std::to_string(user_id);
    MYSQL_RES *quota_result = db->query(quota_query);
    if (!quota_result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    MYSQL_ROW quota_row = mysql_fetch_row(quota_result);
    if (!quota_row)
    {
        mysql_free_result(quota_result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    long long storage_used = std::stoll(quota_row[0] ? quota_row[0] : "0");
    long long storage_limit = std::stoll(quota_row[1] ? quota_row[1] : "0");
    mysql_free_result(quota_result);

    if (storage_used + total_size > storage_limit)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

#ifdef _WIN32
    std::string upload_id = generate_unique_filename("upload").substr(0, 32);
#else
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    std::string upload_id(uuid_str);
#endif

    std::string insert_query =
        "INSERT INTO upload_sessions "
        "(user_id, upload_id, total_chunks, file_size, original_filename, mime_type, expires_at, status) "
        "VALUES (" +
        std::to_string(user_id) + ", '" +
        db->escape_string(upload_id) + "', " +
        std::to_string(total_chunks) + ", " +
        std::to_string(total_size) + ", '" +
        db->escape_string(filename) + "', '" +
        db->escape_string(mime_type) + "', "
        "DATE_ADD(NOW(), INTERVAL 24 HOUR), 'uploading')";

    if (!db->execute(insert_query))
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    db_pool->return_connection(db);
    fs::create_directories(get_temp_dir(upload_id));

    UploadSession session;
    session.upload_id = upload_id;
    session.user_id = user_id;
    session.filename = filename;
    session.total_size = total_size;
    session.total_chunks = total_chunks;
    session.mime_type = mime_type;
    session.status = "uploading";
    return session;
}

bool FileManager::save_chunk(
    const std::string &upload_id,
    int chunk_index,
    const std::string &expected_hash,
    const char *data,
    size_t size)
{
    if (upload_id.empty() || chunk_index < 0 || data == nullptr || size == 0)
    {
        return false;
    }

    auto db = db_pool->get_connection();
    if (!db)
    {
        return false;
    }

    std::string session_query =
        "SELECT total_chunks, status, UNIX_TIMESTAMP(expires_at) "
        "FROM upload_sessions WHERE upload_id = '" +
        db->escape_string(upload_id) + "'";
    MYSQL_RES *session_result = db->query(session_query);
    if (!session_result)
    {
        db_pool->return_connection(db);
        return false;
    }

    MYSQL_ROW session_row = mysql_fetch_row(session_result);
    if (!session_row)
    {
        mysql_free_result(session_result);
        db_pool->return_connection(db);
        return false;
    }

    int total_chunks = std::stoi(session_row[0] ? session_row[0] : "0");
    std::string status = session_row[1] ? session_row[1] : "";
    long long expires_ts = std::stoll(session_row[2] ? session_row[2] : "0");
    mysql_free_result(session_result);

    if (status != "uploading" || expires_ts <= static_cast<long long>(std::time(nullptr)))
    {
        db_pool->return_connection(db);
        return false;
    }

    if (chunk_index >= total_chunks)
    {
        db_pool->return_connection(db);
        return false;
    }

    std::string actual_hash = calculate_sha256(data, size);
    if (!expected_hash.empty() && actual_hash != expected_hash)
    {
        db_pool->return_connection(db);
        return false;
    }

    fs::create_directories(get_temp_dir(upload_id));
    std::string chunk_path = get_chunk_path(upload_id, chunk_index);
    std::ofstream ofs(chunk_path, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        db_pool->return_connection(db);
        return false;
    }
    ofs.write(data, static_cast<std::streamsize>(size));
    ofs.close();

    std::string upsert_query =
        "INSERT INTO upload_chunks "
        "(upload_id, chunk_index, chunk_hash, chunk_size, status, uploaded_at) "
        "VALUES ('" +
        db->escape_string(upload_id) + "', " +
        std::to_string(chunk_index) + ", '" +
        db->escape_string(actual_hash) + "', " +
        std::to_string(size) + ", 'completed', NOW()) "
        "ON DUPLICATE KEY UPDATE "
        "chunk_hash = VALUES(chunk_hash), "
        "chunk_size = VALUES(chunk_size), "
        "status = 'completed', "
        "uploaded_at = NOW()";

    bool ok = db->execute(upsert_query);
    db_pool->return_connection(db);
    return ok;
}

UploadProgress FileManager::get_upload_progress(const std::string &upload_id, int user_id)
{
    UploadProgress progress;
    progress.upload_id = upload_id;
    progress.total_chunks = 0;
    progress.completed_chunks = 0;
    progress.total_size = 0;
    progress.uploaded_size = 0;
    progress.progress = 0.0;

    if (upload_id.empty() || user_id <= 0)
    {
        return progress;
    }

    auto db = db_pool->get_connection();
    if (!db)
    {
        return progress;
    }

    std::string session_query =
        "SELECT total_chunks, file_size, status, UNIX_TIMESTAMP(expires_at) "
        "FROM upload_sessions WHERE upload_id = '" +
        db->escape_string(upload_id) + "' AND user_id = " + std::to_string(user_id);
    MYSQL_RES *session_result = db->query(session_query);
    if (!session_result)
    {
        db_pool->return_connection(db);
        return progress;
    }

    MYSQL_ROW session_row = mysql_fetch_row(session_result);
    if (!session_row)
    {
        mysql_free_result(session_result);
        db_pool->return_connection(db);
        return progress;
    }

    progress.exists = true;
    progress.total_chunks = std::stoi(session_row[0] ? session_row[0] : "0");
    progress.total_size = std::stoll(session_row[1] ? session_row[1] : "0");
    progress.status = session_row[2] ? session_row[2] : "";
    long long expires_ts = std::stoll(session_row[3] ? session_row[3] : "0");
    progress.expired = expires_ts <= static_cast<long long>(std::time(nullptr));
    mysql_free_result(session_result);

    std::string count_query =
        "SELECT COUNT(*), COALESCE(SUM(chunk_size), 0) "
        "FROM upload_chunks WHERE upload_id = '" +
        db->escape_string(upload_id) + "' AND status = 'completed'";
    MYSQL_RES *count_result = db->query(count_query);
    if (count_result)
    {
        MYSQL_ROW row = mysql_fetch_row(count_result);
        if (row)
        {
            progress.completed_chunks = std::stoi(row[0] ? row[0] : "0");
            progress.uploaded_size = std::stoll(row[1] ? row[1] : "0");
        }
        mysql_free_result(count_result);
    }

    std::string failed_query =
        "SELECT chunk_index FROM upload_chunks WHERE upload_id = '" +
        db->escape_string(upload_id) + "' AND status = 'failed' ORDER BY chunk_index ASC";
    MYSQL_RES *failed_result = db->query(failed_query);
    if (failed_result)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(failed_result)))
        {
            progress.failed_chunks.push_back(std::stoi(row[0] ? row[0] : "0"));
        }
        mysql_free_result(failed_result);
    }

    if (progress.total_chunks > 0)
    {
        progress.progress = static_cast<double>(progress.completed_chunks) /
                            static_cast<double>(progress.total_chunks);
    }

    db_pool->return_connection(db);
    return progress;
}

bool FileManager::merge_chunks(const std::string &upload_id,
                               int total_chunks,
                               const std::string &dest_path)
{
    std::ofstream output(dest_path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    std::vector<char> buffer(1024 * 1024);
    for (int i = 0; i < total_chunks; ++i)
    {
        std::ifstream input(get_chunk_path(upload_id, i), std::ios::binary);
        if (!input)
        {
            return false;
        }

        while (input)
        {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize count = input.gcount();
            if (count > 0)
            {
                output.write(buffer.data(), count);
            }
        }
    }

    return true;
}

std::optional<FileInfo> FileManager::complete_upload(
    const std::string &upload_id,
    int user_id)
{
    UploadProgress progress = get_upload_progress(upload_id, user_id);
    if (!progress.exists || progress.expired || progress.status != "uploading")
    {
        return std::nullopt;
    }
    if (progress.completed_chunks != progress.total_chunks)
    {
        return std::nullopt;
    }

    auto db = db_pool->get_connection();
    if (!db)
    {
        return std::nullopt;
    }

    std::string meta_query =
        "SELECT original_filename, mime_type, file_size, total_chunks "
        "FROM upload_sessions WHERE upload_id = '" +
        db->escape_string(upload_id) + "' AND user_id = " + std::to_string(user_id) +
        " AND status = 'uploading'";
    MYSQL_RES *meta_result = db->query(meta_query);
    if (!meta_result)
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    MYSQL_ROW meta_row = mysql_fetch_row(meta_result);
    if (!meta_row)
    {
        mysql_free_result(meta_result);
        db_pool->return_connection(db);
        return std::nullopt;
    }

    std::string original_filename = meta_row[0] ? meta_row[0] : "uploaded_file";
    std::string mime_type = meta_row[1] ? meta_row[1] : "application/octet-stream";
    long long file_size = std::stoll(meta_row[2] ? meta_row[2] : "0");
    int total_chunks = std::stoi(meta_row[3] ? meta_row[3] : "0");
    mysql_free_result(meta_result);

    if (!create_user_directory(user_id))
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    std::string unique_filename = generate_unique_filename(original_filename);
    std::string full_path = get_full_path(user_id, unique_filename);
    if (!merge_chunks(upload_id, total_chunks, full_path))
    {
        db_pool->return_connection(db);
        return std::nullopt;
    }

    if (!db->begin_transaction())
    {
        unlink(full_path.c_str());
        db_pool->return_connection(db);
        return std::nullopt;
    }

    std::string insert_query =
        "INSERT INTO files (user_id, filename, original_filename, file_path, file_size, mime_type) "
        "VALUES (" +
        std::to_string(user_id) + ", '" +
        db->escape_string(unique_filename) + "', '" +
        db->escape_string(original_filename) + "', '" +
        db->escape_string(full_path) + "', " +
        std::to_string(file_size) + ", '" +
        db->escape_string(mime_type) + "')";

    if (!db->execute(insert_query))
    {
        db->rollback();
        unlink(full_path.c_str());
        db_pool->return_connection(db);
        return std::nullopt;
    }

    int file_id = static_cast<int>(db->last_insert_id());
    std::string update_user_query =
        "UPDATE users SET storage_used = storage_used + " + std::to_string(file_size) +
        " WHERE id = " + std::to_string(user_id);
    if (!db->execute(update_user_query))
    {
        db->rollback();
        unlink(full_path.c_str());
        db_pool->return_connection(db);
        return std::nullopt;
    }

    std::string update_session_query =
        "UPDATE upload_sessions SET status = 'completed' "
        "WHERE upload_id = '" +
        db->escape_string(upload_id) + "'";
    if (!db->execute(update_session_query))
    {
        db->rollback();
        unlink(full_path.c_str());
        db_pool->return_connection(db);
        return std::nullopt;
    }

    if (!db->commit())
    {
        db->rollback();
        unlink(full_path.c_str());
        db_pool->return_connection(db);
        return std::nullopt;
    }

    db_pool->return_connection(db);
    fs::remove_all(get_temp_dir(upload_id));

    FileInfo info;
    info.id = file_id;
    info.user_id = user_id;
    info.filename = unique_filename;
    info.original_filename = original_filename;
    info.file_path = full_path;
    info.file_size = file_size;
    info.mime_type = mime_type;
    return info;
}

bool FileManager::cancel_upload(const std::string &upload_id, int user_id)
{
    if (upload_id.empty() || user_id <= 0)
    {
        return false;
    }

    auto db = db_pool->get_connection();
    if (!db)
    {
        return false;
    }

    std::string query =
        "UPDATE upload_sessions SET status = 'cancelled' "
        "WHERE upload_id = '" +
        db->escape_string(upload_id) + "' AND user_id = " + std::to_string(user_id) +
        " AND status = 'uploading'";

    bool ok = db->execute(query);
    db_pool->return_connection(db);
    fs::remove_all(get_temp_dir(upload_id));
    return ok;
}

std::optional<FileInfo> FileManager::upload_file(int user_id,
                                                 const std::string &original_filename,
                                                 const std::string &mime_type,
                                                 const char *data,
                                                 size_t size)
{
    if (!create_user_directory(user_id))
    {
        return std::nullopt;
    }

    std::string unique_filename = generate_unique_filename(original_filename);
    std::string full_path = get_full_path(user_id, unique_filename);

    std::ofstream file(full_path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }

    file.write(data, static_cast<std::streamsize>(size));
    file.close();

    auto db = db_pool->get_connection();
    if (!db)
    {
        unlink(full_path.c_str());
        return std::nullopt;
    }

    std::string query =
        "INSERT INTO files (user_id, filename, original_filename, file_path, file_size, mime_type) "
        "VALUES (" +
        std::to_string(user_id) + ", '" +
        db->escape_string(unique_filename) + "', '" +
        db->escape_string(original_filename) + "', '" +
        db->escape_string(full_path) + "', " +
        std::to_string(size) + ", '" +
        db->escape_string(mime_type) + "')";

    if (!db->execute(query))
    {
        db_pool->return_connection(db);
        unlink(full_path.c_str());
        return std::nullopt;
    }

    int file_id = static_cast<int>(db->last_insert_id());

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

    FileInfo info;
    info.id = file_id;
    info.user_id = user_id;
    info.filename = unique_filename;
    info.original_filename = original_filename;
    info.file_path = full_path;
    info.file_size = static_cast<long long>(size);
    info.mime_type = mime_type;
    return info;
}

std::optional<std::vector<char>> FileManager::download_file(int file_id, int user_id)
{
    auto file_info = get_file_info(file_id, user_id);
    if (!file_info)
    {
        return std::nullopt;
    }

    std::ifstream file(file_info->file_path, std::ios::binary);
    if (!file)
    {
        return std::nullopt;
    }

    std::vector<char> data(static_cast<size_t>(file_info->file_size));
    file.read(data.data(), static_cast<std::streamsize>(file_info->file_size));
    file.close();

    return data;
}

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

bool FileManager::delete_file(int file_id, int user_id)
{
    auto file_info = get_file_info(file_id, user_id);
    if (!file_info)
    {
        return false;
    }

    unlink(file_info->file_path.c_str());

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

std::optional<FileInfo> FileManager::get_shared_file_info(const std::string &code)
{
    auto db = db_pool->get_connection();
    if (!db)
        return std::nullopt;

    std::string query =
        "SELECT id, user_id, filename, original_filename, file_path, "
        "file_size, mime_type, upload_date "
        "FROM files WHERE share_code = '" +
        db->escape_string(code) + "'";

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

long long FileManager::get_user_storage_used(int user_id)
{
    auto db = db_pool->get_connection();
    if (!db)
        return 0;

    std::string query =
        "SELECT SUM(file_size) FROM files WHERE user_id = " +
        std::to_string(user_id);

    MYSQL_RES *result = db->query(query);
    if (!result)
    {
        db_pool->return_connection(db);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    long long total = 0;
    if (row && row[0])
    {
        total = std::stoll(row[0]);
    }

    mysql_free_result(result);
    db_pool->return_connection(db);
    return total;
}

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

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(charset.length() - 1));

    std::string code;
    for (int i = 0; i < length; ++i)
    {
        code += charset[dis(gen)];
    }
    return code;
}
