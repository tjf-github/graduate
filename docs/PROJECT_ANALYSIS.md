# Cloudisk 项目分析文档

## 1. 文档目的

这份文档用于帮助后续分析当前 `lightweight_comm_server` 项目，重点回答下面几个问题：

- 这个项目现在由哪些模块组成
- 服务是如何启动和处理请求的
- 每个模块各自负责什么
- 当前已暴露哪些接口
- 数据库存储了哪些核心实体
- 后续做性能分析、问题排查、功能扩展时，应该优先看哪里

这份文档偏“架构理解与分析入口”，与 `README.md` 的“如何编译运行”定位不同。

## 2. 项目定位

`lightweight_comm_server` 是一个基于 C++17 的轻量级网络通信服务器，
  以文件传输和站内消息作为上层应用场景，验证通信框架在实际业务中的
  可用性。核心技术栈：

- Linux TCP Socket
- 自定义 HTTP 请求解析
- 线程池分发连接处理
- MySQL C API + 连接池
- Session 登录态管理
- 文件落盘到本地存储目录

当前项目已经具备以下主线能力：

- 用户注册、登录、登出
- Session 鉴权
- 用户信息查询和基础资料修改
- 文件上传、下载、列表、删除、重命名、搜索
- 分享码生成与分享下载
- 站内消息短轮询
- 活跃 TCP 连接状态查看

## 3. 目录结构

```text
graduate/
├── include/                  # 核心头文件
├── src/                      # 核心实现
├── static/index.html         # 静态演示页面
├── docs/                     # 项目说明文档
├── init.sql                  # 数据库初始化脚本
├── CMakeLists.txt            # CMake 构建配置
├── Dockerfile                # 容器构建
├── docker-compose.yml        # 容器编排
├── build.sh                  # 构建辅助脚本
├── test_client.py            # 简单测试客户端
└── README.md                 # 项目运行说明
```

## 4. 核心模块总览

### 4.1 Server

文件：

- `include/server.h`
- `src/server.cpp`

职责：

- 创建监听 socket
- 监听端口并 `accept` 客户端连接
- 将每个连接投递到线程池处理
- 调用 `HttpParser` 解析请求
- 调用 `HttpHandler` 执行业务
- 写回 HTTP 响应
- 关闭 socket
- 定时清理过期 Session
- 维护活跃 TCP 连接的 `ClientManager`

特点：

- 当前是“一个连接处理一个 HTTP 请求”的模型
- 响应发送完成后会主动关闭连接
- 没有实现 HTTP keep-alive
- 因为是短连接，所以 `/api/server/status` 看到的是“当前正在处理中的连接”

### 4.2 HttpHandler

文件：

- `include/http_handler.h`
- `src/http_handler.cpp`

职责：

- 路由分发
- 鉴权提取
- JSON 参数解析
- 调用 `UserManager`、`FileManager`、`SessionManager`、`ClientManager`
- 构造统一 HTTP/JSON 响应
- 提供静态文件服务

特点：

- 路由采用 `if` 串行匹配
- JSON 响应格式为：

```json
{
  "code": 200,
  "message": "Success",
  "data": {}
}
```

- 受保护接口通过 `Authorization: Bearer <token>` 获取登录态

### 4.3 ThreadPool

文件：

- `include/thread_pool.h`
- `src/thread_pool.cpp`

职责：

- 创建固定数量工作线程
- 存储待处理任务队列
- 提供 `enqueue()` 提交任务

特点：

- 默认线程数由 `CloudDiskServer` 构造时传入，当前为 10
- 默认最大队列长度为 100
- 队列满时抛异常，而不是阻塞等待
- Worker 内部对任务异常做了 `try/catch`

### 4.4 Database / DBConnectionPool

文件：

- `include/database.h`
- `src/database.cpp`

职责：

- 封装单个 MySQL 连接
- 提供 SQL 执行、查询、事务、字符串转义等能力
- 提供数据库连接池

特点：

- `Database` 使用 MySQL C API
- `DBConnectionPool::get_connection()` 会阻塞等待可用连接
- 返回的连接带有自定义 deleter，作用域结束时会自动归还
- 项目内部同时也存在显式 `return_connection()` 调用，当前实现做了幂等处理

### 4.5 UserManager

文件：

- `include/user_manager.h`
- `src/user_manager.cpp`

职责：

- 用户注册
- 用户登录
- 用户查询
- 用户资料更新
- 存储空间检查
- 用户删除
- 消息发送、消息拉取、消息已读更新

特点：

- 密码当前采用 SHA256 哈希
- 业务实现以直接拼接 SQL 为主，使用 `escape_string()` 做基础保护
- 消息模块基于 `messages` 表实现短轮询方案

### 4.6 FileManager

文件：

- `include/file_manager.h`
- `src/file_manager.cpp`

职责：

- 文件上传
- 文件下载
- 文件元信息查询
- 文件列表分页
- 文件删除
- 文件重命名
- 文件搜索
- 分享码生成
- 分享文件下载

特点：

- 文件真实内容写入本地磁盘
- 数据库存储文件元数据
- 存储路径组织形式为：`storage_path/user_id/unique_filename`
- 上传时生成 UUID 文件名，保留原扩展名

### 4.7 SessionManager

文件：

- `include/session_manager.h`
- `src/session_manager.cpp`

职责：

- 创建 session token
- 校验 token
- 通过 token 获取 user_id
- 删除 session
- 清理过期 session

特点：

- token 存在内存 `std::map` 中
- 当前 session 不落库，服务重启后全部失效
- 默认超时时间为 30 分钟

### 4.8 ClientManager

文件：

- `include/client_manager.h`
- `src/client_manager.cpp`

职责：

- 管理当前活跃 TCP 连接
- 记录 socket fd、IP、端口、连接时间、活跃状态
- 提供连接查询和活跃连接统计

特点：

- 只负责连接层，不负责登录态
- 与 `SessionManager` 职责分离
- 目前主要被 `/api/server/status` 使用

### 4.9 JsonHelper

文件：

- `include/json_helper.h`
- `src/json_helper.cpp`

职责：

- 简化 JSON 序列化
- 简化平面 JSON 对象解析

特点：

- 当前只支持较简单的 JSON 结构
- 更适合平面对象，不适合复杂嵌套结构

### 4.10 Logger

文件：

- `include/logger.h`
- `src/logger.cpp`

职责：

- 单例日志输出
- 同时写控制台和日志文件
- 支持日志级别过滤

## 5. 启动流程

主入口在 `src/main.cpp`。

启动顺序如下：

1. 读取默认配置
2. 用环境变量覆盖端口、存储路径、数据库配置
3. 注册 `SIGINT` 和 `SIGTERM` 信号处理
4. 构造 `CloudDiskServer`
5. `server.start()`
6. 创建监听 socket
7. 启动 Session 清理线程
8. `server.run()` 进入 accept 主循环

相关环境变量：

- `SERVER_PORT`
- `STORAGE_PATH`
- `DB_HOST`
- `DB_USER`
- `DB_PASSWORD`
- `DB_NAME`

## 6. 请求处理链路

一个典型请求的处理路径如下：

1. 客户端建立 TCP 连接
2. `CloudDiskServer::run()` 调用 `accept()`
3. 新连接被加入 `ClientManager`
4. 连接任务提交给 `ThreadPool`
5. `handle_client()` 读取原始 HTTP 数据
6. `HttpParser::parse()` 解析请求行、Header、Query 参数、Body
7. `HttpHandler::handle_request()` 路由分发
8. 路由内部调用对应 Manager 完成业务
9. `HttpParser::build_response()` 构建 HTTP 响应文本
10. 服务器写回响应
11. 连接从 `ClientManager` 移除
12. 关闭 socket

这个链路说明了当前项目的几个分析重点：

- 网络层、协议层、业务层边界比较清晰
- 每个请求都是完整短连接
- 请求并发主要依赖线程池规模和数据库连接池规模

## 7. 当前接口清单

### 7.1 用户相关

- `POST /api/register`
- `POST /api/login`
- `POST /api/logout`
- `GET /api/user/info`
- `PUT /api/user/profile`
- `POST /api/user/profile`

### 7.2 文件相关

- `GET /api/file/list`
- `POST /api/file/upload`
- `GET /api/file/download?id=...`
- `POST /api/file/delete`
- `DELETE /api/file/delete`
- `POST /api/file/rename`
- `PUT /api/file/rename`
- `GET /api/file/search`

### 7.3 分享相关

- `POST /api/share/create`
- `GET /api/share/download?code=...`

### 7.4 消息相关

- `POST /api/message/send`
- `GET /api/message/list?with_user_id=...&limit=...`

### 7.5 服务状态相关

- `GET /api/server/status`

### 7.6 静态资源

- `GET /`
- `GET /index.html`
- `GET /static/...`

## 8. 数据库模型概览

初始化脚本：`init.sql`

当前核心表包括：

- `users`
- `files`
- `messages`
- `share_links`
- `folders`

### 8.1 users

用途：

- 存储用户账号信息、密码哈希、空间占用、空间上限

关键字段：

- `id`
- `username`
- `email`
- `password_hash`
- `storage_used`
- `storage_limit`
- `created_at`

### 8.2 files

用途：

- 存储文件元信息和本地落盘路径

关键字段：

- `id`
- `user_id`
- `filename`
- `original_filename`
- `file_path`
- `file_size`
- `mime_type`
- `share_code`

### 8.3 messages

用途：

- 存储用户间文本消息

关键字段：

- `id`
- `sender_id`
- `receiver_id`
- `content`
- `is_read`
- `created_at`

说明：

- 短轮询读取两人会话记录
- 拉取消息时会将指定发送者发来的消息批量标记已读

### 8.4 share_links

用途：

- 预留更完整的分享能力

说明：

- 当前主要分享流程更多依赖 `files.share_code`
- `share_links` 在现阶段不是唯一核心路径

### 8.5 folders

用途：

- 为文件夹功能预留的数据结构

说明：

- 当前主流程未深度使用

## 9. 当前前后端关系

前端是一个静态演示页：`static/index.html`

当前定位：

- 用于验证和演示后端接口
- 不属于复杂前后端分离项目
- 更接近“后端为主，前端轻展示”

如果后续要做更深分析，建议把前端看成：

- 接口调用样例
- 功能联调入口
- 演示链路验证器

## 10. 构建与运行方式

### 10.1 本地构建

```bash
cd build
cmake ..
make
```

可执行文件：

- `build/lightweight_comm_server`

### 10.2 依赖

- C++17
- Threads
- MySQL client
- OpenSSL
- uuid
- `stdc++fs`

## 11. 当前实现特点

从分析视角看，这个项目有几个很鲜明的实现特点：

### 11.1 优点

- 模块边界相对清楚
- 没有引入复杂框架，便于逐层理解
- 代码路径短，适合课程设计、答辩、演示和二次扩展
- 业务主线完整，用户、文件、分享、消息都已闭环

### 11.2 当前约束

- HTTP 解析器是手写的，能力有限
- JSON 解析器只适合简单场景
- Session 存在内存，重启丢失
- 没有长连接/推送机制，消息依赖短轮询
- SQL 主要是字符串拼接，虽然有转义，但仍偏基础
- 文件 IO、网络 IO、数据库访问都在线程池工作线程里串行完成

## 12. 后续分析时建议重点关注的切面

### 12.1 架构分析

建议优先回答：

- 模块之间是否有循环依赖
- 业务层是否耦合到了协议层
- `Server -> HttpHandler -> Manager` 分层是否稳定

### 12.2 并发分析

建议优先看：

- 线程池大小是否匹配访问压力
- 数据库连接池是否成为瓶颈
- `SessionManager`、`ClientManager`、`Logger` 的锁竞争情况

### 12.3 性能分析

建议优先看：

- 单请求是否存在重复查询
- 文件下载是否会一次性读完整文件进入内存
- 消息短轮询频率高时对数据库的影响

### 12.4 安全分析

建议优先看：

- SQL 拼接路径是否都经过转义
- 手写 HTTP/JSON 解析是否能稳健处理异常输入
- 文件路径和分享下载是否存在绕过风险

### 12.5 可维护性分析

建议优先看：

- 路由是否继续维持 `if` 链，还是演进为路由表
- 错误码是否需要统一
- 是否需要更清晰的 DTO / Response Builder

## 13. 推荐阅读顺序

如果后续有人第一次接手这个项目，建议按下面顺序阅读：

1. `README.md`
2. `src/main.cpp`
3. `include/server.h` + `src/server.cpp`
4. `include/http_handler.h` + `src/http_handler.cpp`
5. `include/user_manager.h` + `src/user_manager.cpp`
6. `include/file_manager.h` + `src/file_manager.cpp`
7. `include/session_manager.h` + `src/session_manager.cpp`
8. `include/database.h` + `src/database.cpp`
9. `init.sql`
10. `static/index.html`

这样可以先建立“整体链路感”，再进入细节。

## 14. 适合下一步补充的文档

如果后续还要继续完善分析资料，建议再新增以下文档：

- 接口时序图文档
- 数据库 ER 图说明
- 模块调用关系图
- 性能瓶颈分析文档
- 安全风险清单
- 部署说明和运行排障手册

## 15. 一句话结论

当前 `lightweight_comm_server` 已经是一个“具备完整主流程、结构清晰、便于继续分析和扩展”的 C++ 轻量级网络通信服务器。后续无论做课程答辩、架构梳理、性能优化还是功能迭代，都可以从 `Server -> HttpHandler -> Manager -> Database/FileSystem` 这条主链路展开。
