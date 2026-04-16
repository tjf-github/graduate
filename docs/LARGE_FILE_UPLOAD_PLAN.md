# 大文件上传扩充方案

## 当前架构分析
- ✗ 整个文件加载到内存（request.body）
- ✗ 单一上传接口，无分块支持
- ✗ 无断点续传机制
- ✗ 无上传进度跟踪

## 改进方案

### 1. 数据库扩充
```sql
-- 上传会话表
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

-- 分块记录表
CREATE TABLE upload_chunks (
    id INT PRIMARY KEY AUTO_INCREMENT,
    upload_id VARCHAR(64) NOT NULL,
    chunk_index INT NOT NULL,
    chunk_hash VARCHAR(64),
    chunk_size BIGINT,
    status ENUM('pending', 'uploading', 'completed', 'failed'),
    uploaded_at TIMESTAMP,
    UNIQUE KEY (upload_id, chunk_index),
    FOREIGN KEY (upload_id) REFERENCES upload_sessions(upload_id)
);
```

### 2. 新增接口

| 接口 | 方法 | 描述 |
|------|------|------|
| /api/file/upload/init | POST | 初始化上传会话 |
| /api/file/upload/chunk | POST | 上传分块 |
| /api/file/upload/complete | POST | 完成上传，合并分块 |
| /api/file/upload/cancel | POST | 取消上传 |
| /api/file/upload/progress | GET | 获取上传进度 |

### 3. 分块大小建议
- 小文件（<100MB）：4MB/块
- 中等文件（100MB-1GB）：8-16MB/块
- 大文件（>1GB）：32-64MB/块

### 4. 前端上传流程
1. POST /api/file/upload/init → 获取 upload_id
2. 并发 POST /api/file/upload/chunk (3-5个并发)
3. GET /api/file/upload/progress → 实时进度
4. POST /api/file/upload/complete → 合并文件

### 5. 关键特性
- ✅ 分块上传支持
- ✅ 断点续传 (24小时)
- ✅ SHA256校验
- ✅ 并发上传 (3-5个)
- ✅ 进度追踪
- ✅ 自动重试
