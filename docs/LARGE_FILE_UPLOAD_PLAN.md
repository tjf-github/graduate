# 大文件分块上传与流式下载方案

> 本文档基于 [../CODEX_LARGE_FILE_IMPL_INSTRUCTIONS.md](../CODEX_LARGE_FILE_IMPL_INSTRUCTIONS.md) 整理，目标是把实现要求翻译成可直接执行的文档方案。

## 1. 目标

在不修改现有普通上传下载接口行为的前提下，新增一套适用于大文件的上传与下载能力：

- 分块上传
- 会话追踪
- 进度查询
- 取消上传
- 完成合并
- 流式下载

## 2. 现有问题

当前普通上传实现的关键问题是整文件进入内存：

```cpp
std::string data = request.body;
```

这会导致大文件上传时内存不可控。下载路径也存在类似问题，因为当前会把完整文件读入容器再返回。

## 3. 设计原则

- 不替换现有接口，只新增新接口族
- 上传分块时不新增 body 副本
- 合并和下载必须流式处理
- 所有数据库访问继续通过连接池
- 临时文件和完成文件目录分离

## 4. 数据库变更

在 `init.sql` 末尾追加：

```sql
CREATE TABLE IF NOT EXISTS upload_sessions (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    upload_id VARCHAR(64) UNIQUE NOT NULL,
    total_chunks INT NOT NULL,
    file_size BIGINT NOT NULL,
    original_filename VARCHAR(255) NOT NULL,
    mime_type VARCHAR(100) DEFAULT '',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    status ENUM('uploading', 'completed', 'cancelled') DEFAULT 'uploading',
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS upload_chunks (
    id INT PRIMARY KEY AUTO_INCREMENT,
    upload_id VARCHAR(64) NOT NULL,
    chunk_index INT NOT NULL,
    chunk_hash VARCHAR(64) DEFAULT '',
    chunk_size BIGINT NOT NULL,
    status ENUM('pending', 'completed', 'failed') DEFAULT 'pending',
    uploaded_at TIMESTAMP NULL,
    UNIQUE KEY uq_upload_chunk (upload_id, chunk_index),
    FOREIGN KEY (upload_id) REFERENCES upload_sessions(upload_id) ON DELETE CASCADE
);
```

## 5. CMake 变更

需要确认：

```cmake
find_package(OpenSSL REQUIRED)
```

并在 `target_link_libraries` 中包含：

```cmake
OpenSSL::Crypto
OpenSSL::SSL
```

## 6. FileManager 扩充

### 6.1 需要补充的数据结构

- `UploadSession`
- `UploadProgress`

### 6.2 需要补充的接口

- `create_upload_session(...)`
- `upload_chunk(...)`
- `merge_chunks(...)`
- `get_upload_progress(...)`
- `cancel_upload(...)`

### 6.3 需要补充的工具方法

- `get_chunk_dir(...)`
- `get_chunk_path(...)`
- `calculate_sha256(...)`

## 7. HttpHandler 路由扩充

新增路由：

- `POST /api/file/upload/init`
- `POST /api/file/upload/chunk`
- `POST /api/file/upload/complete`
- `POST /api/file/upload/cancel`
- `GET /api/file/upload/progress`
- `GET /api/file/download/stream`

## 8. 各接口行为

### 初始化上传

请求：

```json
{
  "filename": "large.zip",
  "total_size": 1073741824,
  "total_chunks": 256,
  "mime_type": "application/zip"
}
```

成功响应：

```json
{
  "code": 200,
  "message": "Success",
  "data": {
    "upload_id": "xxx",
    "chunk_size": 4194304,
    "total_chunks": 256
  }
}
```

### 上传分块

请求路径示例：

```text
/api/file/upload/chunk?upload_id=xxx&chunk_index=0&chunk_hash=...
```

要求：

- token 校验
- query 参数校验
- 直接传 `request.body.data()` 给文件层
- 哈希不匹配返回 `400`

### 查询进度

响应示例：

```json
{
  "code": 200,
  "data": {
    "upload_id": "xxx",
    "total_chunks": 256,
    "completed_chunks": 128,
    "progress": 0.5
  }
}
```

### 完成上传

要求：

- 确认所有分块均完成
- 顺序合并到最终目录
- 写入 `files` 表
- 更新 `users.storage_used`
- 更新 `upload_sessions.status='completed'`
- 清理临时目录

### 取消上传

要求：

- 校验归属
- 更新状态为 `cancelled`
- 删除临时目录

### 流式下载

优先方案：

- 构造响应头
- 分块读取文件
- 直接向 client socket 发送

保底方案：

- 至少不要一次性 `read` 整个文件
- 使用固定大小缓冲循环读取

## 9. 前端变更

需要在 `static/index.html` 的上传区域旁边追加：

- 大文件选择按钮
- 进度条
- 进度百分比
- 取消上传按钮
- 状态提示文本

前端默认建议：

- `CHUNK_SIZE = 4 * 1024 * 1024`
- `CONCURRENCY = 3`

## 10. 存储结构

```text
{storage_path}/
├── {user_id}/
│   └── {uuid_filename}
└── uploads/
    └── {upload_id}/
        ├── chunk_0
        ├── chunk_1
        └── chunk_N
```

## 11. 关键约束清单

- 不删除现有接口
- `upload_chunk` 不复制 body
- 合并时使用 `std::vector<char>` 作为缓冲
- 临时目录创建使用 `std::filesystem::create_directories()`
- 过期会话统一返回 `410`
- 前端复用已有 `getToken()` 和 `refreshFileList()`

## 12. 实现备注

### FileManager 需要补的点

- 创建上传会话
- 写入单个分块
- 查询分块进度
- 合并分块为最终文件
- 取消上传并清理临时目录
- 计算 SHA256 校验值

### HttpHandler 需要补的点

- 新增大文件上传处理器声明
- 在路由 `if` 链中接入新接口
- 保持现有接口不受影响
- 为过期、配额不足、未完成分块等场景返回明确错误码

### 当前代码状态提醒

- `include/file_manager.h` 已经有部分分块上传结构
- `src/file_manager.cpp` 仍存在未完成实现
- `include/http_handler.h` 尚未声明新增处理器
- `src/http_handler.cpp` 尚未接入新增路由

## 13. 集成步骤

1. 更新 `init.sql`，追加上传会话和分块表。
2. 更新 `CMakeLists.txt`，确认链接 OpenSSL。
3. 在 `include/file_manager.h` 和 `src/file_manager.cpp` 中补齐接口与实现。
4. 在 `include/http_handler.h` 和 `src/http_handler.cpp` 中补齐处理器和路由。
5. 在 `static/index.html` 中接入大文件上传 UI 和脚本逻辑。
6. 验证普通上传不回归，再验证分块上传全流程。

## 14. API 摘要

### 初始化上传

```http
POST /api/file/upload/init
```

### 上传分块

```http
POST /api/file/upload/chunk?upload_id=abc123&chunk_index=0&chunk_hash=...
```

### 查询进度

```http
GET /api/file/upload/progress?upload_id=abc123
```

### 完成上传

```http
POST /api/file/upload/complete?upload_id=abc123
```

### 取消上传

```http
POST /api/file/upload/cancel?upload_id=abc123
```

### 流式下载

```http
GET /api/file/download/stream?id=123
```

关键返回语义：

- `401`：token 无效
- `400`：参数错误、哈希不匹配、分块未完成
- `410`：上传会话过期
- `507`：存储配额不足

## 15. 完成后的自检

- 普通上传接口仍然可用
- 初始化接口能正常返回 `upload_id`
- 分块上传后数据库状态能正确更新
- 进度接口能正确计算完成比例
- 完成上传后最终文件写入 `files` 表
- 取消上传后临时目录被删除
- 下载大文件时内存占用不再随文件大小线性增长

## 16. 不在本次范围内

- 断点续传的前端重启恢复
- 过期会话后台定时清理任务
- HTTP Range 请求
