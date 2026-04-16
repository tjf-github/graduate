#ifndef LARGE_FILE_UPLOAD_H
#define LARGE_FILE_UPLOAD_H

#include <string>
#include <vector>
#include <optional>
#include <ctime>

// 上传会话结构体
struct UploadSession {
    std::string upload_id;
    int user_id;
    std::string filename;
    long long total_size;
    int total_chunks;
    std::string mime_type;
};

// 分块信息结构体
struct ChunkInfo {
    std::string upload_id;
    int chunk_index;
    std::string chunk_hash;
    long long chunk_size;
    std::string status;
};

// 上传进度结构体
struct UploadProgress {
    std::string upload_id;
    int total_chunks;
    int completed_chunks;
    long long total_size;
    long long uploaded_size;
    double progress;
    std::vector<int> failed_chunks;
};

#endif // LARGE_FILE_UPLOAD_H
