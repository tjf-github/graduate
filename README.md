# lightweight_comm_server

lightweight_comm_server 是一个基于 C++ Socket 的轻量级网络通信服务器，支持多客户端并发连接、文件传输与站内消息收发。项目当前已经具备“可编译、可运行、可演示”的基础能力，包含用户系统、文件管理、分享下载和站内消息短轮询。

## 项目现状

### 已实现能力
- 用户注册、登录、登出
- Session 鉴权，`Authorization: Bearer <token>` 访问受保护接口
- 用户资料查看与基础资料修改
- 文件上传、下载、列表、删除、重命名、搜索
- 文件分享链接生成与通过分享码下载
- 站内文本消息发送、会话拉取、已读更新
- 简单静态前端页面，支持登录、资料展示、上传下载、分享和消息演示

### 当前技术栈
- C++17
- 原生 TCP socket + 自定义 HTTP 解析
- 线程池
- MySQL C API
- OpenSSL SHA256
- 静态前端页面 [static/index.html](/home/tjf/graduate/static/index.html)

## 项目结构

```text
cloudisk/
├── include/              # 头文件
├── src/                  # 核心实现
├── static/index.html     # 简单前端演示页
├── init.sql              # 数据库初始化脚本
├── CMakeLists.txt        # CMake 构建配置
├── build.sh              # 编译/部署辅助脚本
├── README.md             # 当前项目说明
└── docs/FEATURE_OUTLINE.md
```

## 快速开始

### 1. 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libmysqlclient-dev libssl-dev uuid-dev mysql-server
```

### 2. 初始化数据库

```bash
sudo service mysql start
mysql -u root -p < init.sql
```

### 3. 配置运行环境

项目支持通过环境变量覆盖默认配置，也可以直接复制 `.env.example` 为 `.env` 后统一维护：

```bash
cp .env.example .env
export SERVER_PORT=8080
export STORAGE_PATH=./storage
export STATIC_DIR=./static
export DB_HOST=localhost
export DB_PORT=3306
export DB_USER=cloudisk
export DB_PASSWORD=change_me
export DB_NAME=cloudisk
```

说明：
- 如果不设置环境变量，程序会使用 [src/main.cpp](/home/tjf/graduate/src/main.cpp) 中的默认值。
- `STORAGE_PATH` 指向文件实际落盘目录。
- `STATIC_DIR` 用于部署时显式指定静态资源目录，Docker 场景下建议保持 `/app/static`。
- `DB_PORT` 现在也支持通过环境变量覆盖，便于容器或云数据库部署。

### 4. 编译

```bash
cmake -S . -B build
cmake --build build
```

### 5. 启动

```bash
./build/lightweight_comm_server
```

启动后默认访问地址：

```text
http://localhost:8080
```

浏览器直接打开根路径即可加载静态页面：

```text
http://localhost:8080/
```

## Docker 部署

仓库已经内置 `Dockerfile` 和 `docker-compose.yml`，并补齐了容器运行所需的静态资源复制与环境变量模板：

```bash
cp .env.example .env
docker compose up --build -d
```

说明：
- `docker-compose.yml` 支持通过 `.env` 覆盖 `SERVER_PORT`、`DB_NAME`、`MYSQL_ROOT_PASSWORD` 等参数。
- 应用容器内默认使用 `STATIC_DIR=/app/static`，前端页面可直接通过根路径访问。
- 文件存储会落到 Compose volume `storage_data` 中。

## 数据库概览

`init.sql` 当前会初始化以下核心表：
- `users`
- `files`
- `messages`
- `share_links`
- `folders`

其中消息能力使用 `messages` 表，分享能力当前主要复用 `files.share_code` 与分享下载接口。

## 接口约定

### 通用响应格式

当前后端统一返回：

```json
{
  "code": 200,
  "message": "Success",
  "data": {}
}
```

说明：
- `code` 同时作为业务状态码和 HTTP 状态码主体含义
- `data` 仅在需要时返回
- 受保护接口需要 `Authorization: Bearer <token>`

## 核心 API

### 用户相关

#### `POST /api/register`

请求体：

```json
{
  "username": "testuser",
  "email": "test@example.com",
  "password": "test123"
}
```

#### `POST /api/login`

请求体：

```json
{
  "username": "testuser",
  "password": "test123"
}
```

成功响应示例：

```json
{
  "code": 200,
  "message": "Login success",
  "data": {
    "id": 1,
    "token": "session_token",
    "username": "testuser"
  }
}
```

#### `POST /api/logout`

请求头：

```text
Authorization: Bearer <token>
```

#### `GET /api/user/info`

返回用户 ID、用户名、邮箱、空间占用、注册时间、当前会话数和超时策略。

#### `PUT /api/user/profile`

请求体：

```json
{
  "username": "new_name",
  "email": "new_email@example.com"
}
```

### 文件相关

#### `GET /api/file/list?offset=0&limit=10`
- 返回当前用户文件列表

#### `POST /api/file/upload`

请求头：

```text
Authorization: Bearer <token>
X-Filename: test.txt
```

请求体为原始二进制文件内容。

#### `GET /api/file/download?id=1`
- 下载当前用户自己的文件

#### `POST /api/file/delete`

请求体：

```json
{
  "id": 1
}
```

#### `POST /api/file/rename`

请求体：

```json
{
  "id": 1,
  "new_name": "renamed.txt"
}
```

#### `GET /api/file/search?keyword=test`
- 按文件名关键字搜索当前用户文件

### 分享相关

#### `POST /api/share/create`

请求体：

```json
{
  "file_id": 1
}
```

成功后返回 `share_code` 和 `share_url`。

#### `GET /api/share/download?code=xxxx`
- 无需登录即可通过分享码下载文件

### 消息相关

#### `POST /api/message/send`

请求体：

```json
{
  "receiver_id": 2,
  "content": "hello"
}
```

#### `GET /api/message/list?with_user_id=2&limit=50`
- 返回当前用户与指定用户的双向消息
- 默认按时间倒序返回，前端展示时会转成正序
- 拉取时会自动将“对方向当前用户发送”的消息标记为已读

## 快速验收示例

### 1. 注册

```bash
curl -X POST http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"user_a","email":"a@test.com","password":"test123"}'
```

### 2. 登录

```bash
TOKEN=$(curl -s -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"user_a","password":"test123"}' | jq -r '.data.token')
```

### 3. 文件列表

```bash
curl -X GET http://localhost:8080/api/file/list \
  -H "Authorization: Bearer $TOKEN"
```

### 4. 发送消息

```bash
curl -X POST http://localhost:8080/api/message/send \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"receiver_id":2,"content":"hello"}'
```

### 5. 拉取消息

```bash
curl -X GET "http://localhost:8080/api/message/list?with_user_id=2&limit=20" \
  -H "Authorization: Bearer $TOKEN"
```

## 当前前端演示范围

[static/index.html](/home/tjf/graduate/static/index.html) 当前支持：
- 注册 / 登录
- 查看个人资料与空间占用
- 修改用户名和邮箱
- 上传 / 下载 / 删除文件
- 生成分享链接并通过链接下载
- 输入用户 ID 进行站内消息收发与短轮询刷新

说明：
- 文件重命名、文件搜索后端已实现，但页面尚未提供独立操作入口。
- 消息模块为“最小可演示版本”，不包含联系人列表、未读角标和通知中心。

## 已知限制

- HTTP 解析和 JSON 解析都比较轻量，只覆盖当前项目需要的基础场景
- SQL 主要通过字符串拼接完成，已做 `escape_string`，但未全面使用 prepared statement
- Session 存在内存中，服务重启后会失效
- 分享能力目前是基础版，不包含过期时间、提取码、访问统计
- 前端是单页静态演示，不是完整生产级管理台

## 后续规划

后续迭代方向已整理到 [docs/FEATURE_OUTLINE.md](/home/tjf/graduate/docs/FEATURE_OUTLINE.md)，区分了“当前已完成能力”和“下一阶段建议优先级”。
