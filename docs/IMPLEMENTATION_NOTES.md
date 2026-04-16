# 大文件上传实现说明

## 核心实现要点

### 1. FileManager 中添加的方法

```cpp
// 初始化上传会话
std::optional<UploadSession> create_upload_session(
    int user_id,
    const std::string& filename,
    long long total_size,
    int total_chunks,
    const std::string& mime_type
);

// 上传分块 - 关键是流式处理，不加载至内存
bool upload_chunk(
    const std::string& upload_id,
    int chunk_index,
    const char* data,
    size_t chunk_size,
    const std::string& chunk_hash
);

// 合并分块 - 流式读取分块，顺序写入最终文件
std::optional<FileInfo> merge_chunks(const std::string& upload_id);

// 获取进度
std::optional<UploadProgress> get_upload_progress(const std::string& upload_id);

// 取消上传 - 清理临时文件和数据库记录
bool cancel_upload(const std::string& upload_id);
```

### 2. HttpHandler 中添加的路由

```cpp
// POST /api/file/upload/init
HttpResponse handle_upload_init(const HttpRequest &request);

// POST /api/file/upload/chunk
HttpResponse handle_upload_chunk(const HttpRequest &request);

// GET /api/file/upload/progress
HttpResponse handle_upload_progress(const HttpRequest &request);

// POST /api/file/upload/complete
HttpResponse handle_upload_complete(const HttpRequest &request);

// POST /api/file/upload/cancel
HttpResponse handle_upload_cancel(const HttpRequest &request);
```

### 3. 关键实现细节

#### 流式处理分块
```cpp
// 而不是：
// std::string data = request.body;  // 危险！

// 应该是：
std::ofstream chunk_file(chunk_path, std::ios::binary);
chunk_file.write(data, chunk_size);  // 直接写入
```

#### SHA256 校验
```cpp
#include <openssl/sha.h>

std::string calculate_sha256(const char* data, size_t size) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, (unsigned char*)data, size);
    SHA256_Final(hash, &sha256);
    // 转换为十六进制字符串
}
```

#### 分块合并
```cpp
// 使用缓冲区流式读写
const size_t BUFFER_SIZE = 1024 * 1024;  // 1MB
char buffer[BUFFER_SIZE];

for (int i = 0; i < total_chunks; i++) {
    std::ifstream chunk_file(get_chunk_path(upload_id, i), std::ios::binary);
    while (chunk_file.read(buffer, BUFFER_SIZE) || chunk_file.gcount() > 0) {
        final_file.write(buffer, chunk_file.gcount());
    }
}
```

## 依赖配置

### CMakeLists.txt
```cmake
find_package(OpenSSL REQUIRED)
target_link_libraries(cloudisk_server PRIVATE OpenSSL::Crypto OpenSSL::SSL)
```

### 系统依赖
- Ubuntu/Debian: `sudo apt-get install libssl-dev`
- macOS: `brew install openssl`

## 性能优化建议

### 1. 并发数优化
- 网络好 (>100Mbps): concurrency=5
- 网络一般 (50-100Mbps): concurrency=3
- 网络差 (<50Mbps): concurrency=1-2

### 2. 分块大小优化
- SSD 硬盘: 32MB (更快写入)
- HDD 硬盘: 8MB (减少寻道)
- 慢速网络: 1-4MB

### 3. 存储结构优化
```
storage/
├── user_id/
│   ├── files/          # 完成的文件
│   └── uploads/        # 临时分块
│       └── upload_id/
│           ├── chunk_0
│           ├── chunk_1
│           └── ...
```

## 容错处理

### 网络中断
- 保存 upload_id 和 chunk 索引
- 恢复时查询数据库获取已完成的分块
- 只重新上传失败的分块

### 分块校验失败
- 返回错误提示
- 保留失败的分块供重试
- 自动重试最多3次

### 存储空间不足
- 上传前检查剩余空间
- 合并前再检查一次
- 超配时返回错误并清理临时文件

## 测试场景

### 正常上传
```bash
python3 test_large_upload.py --file test.zip --token xxx
```

### 大文件上传
```bash
# 创建100MB测试文件
dd if=/dev/urandom of=test_100mb bs=1M count=100
python3 test_large_upload.py --file test_100mb --chunk-size 16M
```

### 中断恢复
```bash
# 第一次 (中断)
python3 test_large_upload.py --file large_file.zip --token xxx

# 查看输出中的 upload_id，然后恢复
```

## 监控和调试

### 检查上传会话
```sql
SELECT * FROM upload_sessions WHERE expires_at > NOW();
```

### 检查分块进度
```sql
SELECT chunk_index, status FROM upload_chunks 
WHERE upload_id = 'xxx' ORDER BY chunk_index;
```

### 清理过期会话
```sql
DELETE FROM upload_sessions WHERE expires_at < NOW();
```

## 常见问题排查

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| 内存溢出 | 分块加载到内存 | 使用流式处理 |
| 哈希不匹配 | 网络传输错误 | 自动重试 |
| 合并失败 | 分块丢失 | 检查临时目录权限 |
| 进度为0 | 会话过期 | 重新初始化上传 |

## 部署建议

### 生产环境
1. 设置合理的文件大小限制 (10GB)
2. 启用会话过期自动清理 (24小时)
3. 监控服务器磁盘空间
4. 定期备份数据库
5. 启用HTTPS防止传输被截断

### 高并发场景
1. 增加数据库连接池大小
2. 使用专用磁盘存储分块
3. 启用分块缓存机制
4. 监控系统负载
5. 考虑使用CDN加速
