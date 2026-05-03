# Cloudisk 项目综合总结

> 这份文档整合项目背景、模块结构、当前功能状态和大文件上传扩充方向，作为仓库总览页使用。

## 1. 项目定位

`lightweight_comm_server` 是一个基于 C++17 的轻量级云盘后端，核心目标是以“文件传输 + 站内消息”为业务场景，验证自研网络服务框架在课程设计或毕业设计中的可实现性。

核心技术栈：

- Linux Socket + 自定义 HTTP 解析
- 固定线程池
- MySQL C API + 连接池
- Session 登录态管理
- 文件系统本地落盘
- 静态 HTML 演示页

## 2. 目录结构

```text
graduatenew/
├── include/
├── src/
├── static/
├── docs/
├── deploy/
├── init.sql
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── build.sh
└── README.md
```

## 3. 模块总览

### Server
- 负责监听 socket、接收连接、投递线程池、写回响应。
- 当前是“一请求一连接”的短连接模型。

### HttpHandler
- 负责路由分发、鉴权提取、参数解析和统一 JSON 响应。
- 当前使用 `if` 串行匹配路径。

### UserManager
- 负责用户注册、登录、资料查询更新。
- 同时承载站内消息的发送、拉取、已读处理。

### FileManager
- 负责文件上传、下载、列表、删除、重命名、搜索和分享码生成。
- 现有普通上传仍存在整文件进入内存的问题。

### SessionManager
- 使用内存 map 保存 token 到 user_id 的映射。
- 默认 30 分钟超时，服务重启后 session 失效。

### Database / DBConnectionPool
- 基于 MySQL C API 封装连接与连接池。
- 调用层通过 RAII 归还连接。

## 4. 请求处理链路

```text
TCP 连接
→ accept()
→ ThreadPool
→ HttpParser::parse()
→ HttpHandler::handle_request()
→ UserManager / FileManager / SessionManager
→ HttpParser::build_response()
→ write()
→ close()
```

## 5. 当前接口清单

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
- `GET /api/message/list?with_user_id=...&limit=...`

### 其他
- `GET /api/server/status`
- `GET /`
- `GET /index.html`
- `GET /static/...`

## 6. 当前功能完成状态

### 已完成
- 用户注册、登录、登出、Session 鉴权
- 用户信息查看与基础资料修改
- 文件上传、下载、列表、删除、重命名、搜索
- 分享码生成与分享下载
- 站内消息发送、拉取、已读更新
- 静态前端演示页

### 已知约束
- 普通上传接口一次性读取 `request.body`
- 普通下载接口也会把文件完整读入内存
- Session 不落库
- HTTP 与 JSON 解析均为手写简化实现

## 7. 大文件上传扩充目标

根据 [../CODEX_LARGE_FILE_IMPL_INSTRUCTIONS.md](../CODEX_LARGE_FILE_IMPL_INSTRUCTIONS.md) 的要求，本次需要在不破坏现有接口的前提下追加：

- 分块上传会话初始化
- 分块写入
- 上传进度查询
- 分块合并完成
- 取消上传
- 流式下载能力

对应新增接口：

- `POST /api/file/upload/init`
- `POST /api/file/upload/chunk`
- `GET /api/file/upload/progress`
- `POST /api/file/upload/complete`
- `POST /api/file/upload/cancel`
- `GET /api/file/download/stream`

## 8. 文件存储结构

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

合并成功或取消上传后，需要删除 `uploads/{upload_id}/` 整个临时目录。

## 9. 建议阅读顺序

1. [README.md](../README.md)
2. [PROJECT_ANALYSIS.md](PROJECT_ANALYSIS.md)
3. [LARGE_FILE_UPLOAD_PLAN.md](LARGE_FILE_UPLOAD_PLAN.md)
4. [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)

## 10. 一句话结论

Cloudisk 已经具备完整的基础业务主线，本次文档整理的重点是把“现状、限制、扩充方案、部署入口”统一到 `docs/`，为后续补完大文件上传和答辩展示提供稳定基线。
