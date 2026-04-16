# 大文件上传集成指南

## 步骤 1: 数据库初始化

执行以下 SQL 语句创建必需的表：

```sql
CREATE TABLE upload_sessions (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    upload_id VARCHAR(64) UNIQUE NOT NULL,
    total_chunks INT NOT NULL,
    file_size BIGINT NOT NULL,
    original_filename VARCHAR(255) NOT NULL,
    mime_type VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE upload_chunks (
    id INT PRIMARY KEY AUTO_INCREMENT,
    upload_id VARCHAR(64) NOT NULL,
    chunk_index INT NOT NULL,
    chunk_hash VARCHAR(64),
    chunk_size BIGINT,
    status ENUM('pending', 'uploading', 'completed', 'failed') DEFAULT 'pending',
    uploaded_at TIMESTAMP,
    UNIQUE KEY (upload_id, chunk_index),
    FOREIGN KEY (upload_id) REFERENCES upload_sessions(upload_id)
);
```

## 步骤 2: 更新 CMakeLists.txt

```cmake
find_package(OpenSSL REQUIRED)

target_link_libraries(cloudisk_server 
    PRIVATE
    ${MYSQL_LIBRARIES}
    pthread
    OpenSSL::Crypto
    OpenSSL::SSL
)
```

## 步骤 3: 集成代码

1. 将 `include/large_file_upload.h` 的内容添加到 `include/file_manager.h`
2. 复制 FileManager 扩充实现到 `src/file_manager.cpp`
3. 复制 HttpHandler 处理器到 `src/http_handler.cpp`

## 步骤 4: 前端集成

在 `static/index.html` 中添加：

```html
<script src="/static/large_file_uploader.js"></script>
<script>
const uploader = new LargeFileUploader({
    baseUrl: '/api',
    chunkSize: 4 * 1024 * 1024,
    concurrency: 3
});
</script>
```

## 步骤 5: 编译

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## 步骤 6: 测试

```bash
python3 test_large_upload.py \
  --file /path/to/large_file.zip \
  --server http://localhost:8080 \
  --token YOUR_SESSION_TOKEN
```
