# Cloudisk 项目分析文档

## 1. 文档目的

这份文档回答四个问题：

- 项目由哪些模块组成
- 服务如何启动并处理请求
- 当前接口和数据模型覆盖了什么
- 后续做大文件上传扩充时应优先看哪里

## 2. 项目定位

Cloudisk 是一个基于 C++17 的轻量级网络通信服务，核心业务围绕用户、文件、分享和站内消息展开。它更接近“后端主导、前端轻展示”的教学型或展示型项目，而不是完整的前后端分离产品。

## 3. 目录结构

```text
graduatenew/
├── include/                  # 核心头文件
├── src/                      # 核心实现
├── static/index.html         # 静态演示页面
├── docs/                     # 项目说明文档
├── init.sql                  # 数据库初始化脚本
├── CMakeLists.txt            # CMake 构建配置
├── Dockerfile                # 容器构建
├── docker-compose.yml        # 容器编排
├── build.sh                  # 构建辅助脚本
└── test_large_upload.py      # 大文件上传测试脚本
```

## 4. 核心模块

### 4.1 Server

相关文件：
- `include/server.h`
- `src/server.cpp`

职责：
- 创建监听 socket
- `accept` 客户端连接
- 将连接投递给线程池
- 驱动 `HttpParser` 和 `HttpHandler`
- 清理过期 Session
- 维护 `ClientManager`

### 4.2 HttpHandler

相关文件：
- `include/http_handler.h`
- `src/http_handler.cpp`

职责：
- 路由分发
- `Authorization: Bearer <token>` 鉴权
- JSON 解析和 JSON 响应构造
- 调用 `UserManager`、`FileManager`、`SessionManager`
- 静态文件服务

特点：
- 路由采用 `if` 串行匹配
- 统一响应格式为：

```json
{
  "code": 200,
  "message": "Success",
  "data": {}
}
```

### 4.3 Database / DBConnectionPool

相关文件：
- `include/database.h`
- `src/database.cpp`

职责：
- 封装 MySQL 连接与查询
- 管理连接池
- 提供字符串转义与事务能力

### 4.4 UserManager

相关文件：
- `include/user_manager.h`
- `src/user_manager.cpp`

职责：
- 注册、登录、资料更新
- 消息发送、消息拉取、已读更新

### 4.5 FileManager

相关文件：
- `include/file_manager.h`
- `src/file_manager.cpp`

职责：
- 普通文件上传下载
- 元信息查询、列表、删除、重命名、搜索
- 分享码生成与分享下载

当前需要注意：
- `upload_file()` 使用整段 body 写盘
- `download_file()` 会把完整文件读入 `std::vector<char>`
- 大文件上传扩充代码处于“结构已开始、实现未完成”的状态

### 4.6 SessionManager / ClientManager / Logger

- `SessionManager` 负责 token 生命周期
- `ClientManager` 负责活跃 TCP 连接列表
- `Logger` 负责日志输出和级别过滤

## 5. 启动流程

主入口位于 `src/main.cpp`，整体顺序如下：

1. 读取默认配置
2. 用环境变量覆盖端口、存储路径和数据库参数
3. 构造 `CloudDiskServer`
4. 启动监听 socket
5. 创建 Session 清理线程
6. 进入 `accept` 主循环

常见环境变量：

- `SERVER_PORT`
- `STORAGE_PATH`
- `STATIC_DIR`
- `DB_HOST`
- `DB_PORT`
- `DB_USER`
- `DB_PASSWORD`
- `DB_NAME`

## 6. 请求处理链路

```text
accept()
→ ThreadPool::enqueue()
→ handle_client()
→ HttpParser::parse()
→ HttpHandler::handle_request()
→ Manager 层处理业务
→ HttpParser::build_response()
→ 发送响应
→ 关闭连接
```

## 7. 当前接口盘点

### 用户
- `POST /api/register`
- `POST /api/login`
- `POST /api/logout`
- `GET /api/user/info`
- `PUT /api/user/profile`
- `POST /api/user/profile`

### 文件
- `GET /api/file/list`
- `POST /api/file/upload`
- `GET /api/file/download?id=...`
- `POST /api/file/delete`
- `DELETE /api/file/delete`
- `POST /api/file/rename`
- `PUT /api/file/rename`
- `GET /api/file/search`

### 分享
- `POST /api/share/create`
- `GET /api/share/download?code=...`

### 消息
- `POST /api/message/send`
- `GET /api/message/list`

### 服务状态和静态资源
- `GET /api/server/status`
- `GET /`
- `GET /index.html`
- `GET /static/...`

## 8. 数据库模型

当前核心表：

- `users`
- `files`
- `messages`
- `share_links`
- `folders`

大文件上传扩充后还需要追加：

- `upload_sessions`
- `upload_chunks`

## 9. 当前弱点

- 普通上传和下载都存在大文件内存压力
- Session 在内存中，重启后失效
- SQL 主要以字符串拼接为主
- 手写 HTTP/JSON 解析器容错有限
- 路由扩展仍依赖大段 `if` 链

## 10. 当前能力与后续方向

### 当前已完成

- 用户注册、登录、登出与 Session 鉴权
- 用户资料查看与基础资料修改
- 文件上传、下载、列表、删除、重命名、搜索
- 分享码生成与分享下载
- 站内消息发送、消息拉取与已读更新
- 静态前端演示页

### 当前前端覆盖较弱

- 文件重命名入口
- 文件搜索入口
- 服务状态展示

### 下一阶段建议

- 优先补完大文件分块上传与流式下载
- 再补前端的重命名、搜索和分享管理入口
- 之后考虑文件夹、批量操作、回收站和通知中心

## 11. 分析和扩充时的优先阅读点

如果要继续实现大文件上传，建议先看：

1. `include/file_manager.h`
2. `src/file_manager.cpp`
3. `include/http_handler.h`
4. `src/http_handler.cpp`
5. `init.sql`
6. `static/index.html`
7. [LARGE_FILE_UPLOAD_PLAN.md](LARGE_FILE_UPLOAD_PLAN.md)

## 12. 结论

这个项目的业务闭环已经形成，但大文件处理仍是最明显的能力缺口。本次文档体系的价值，就是把“当前真实状态”和“目标扩充方案”放进同一个可持续维护的目录里。
