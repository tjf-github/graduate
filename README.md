# 云盘系统 - C++ 实现

一个高性能、多线程的云盘系统，使用C++实现，支持用户管理和文件存储功能。

## 功能特性

### ✅ 已实现功能
- **多线程处理**: 使用线程池处理并发请求
- **用户系统**: 
  - 用户注册、登录、登出
  - 密码加密存储 (SHA256)
  - 会话管理（30分钟超时）
  - 存储配额管理（默认10GB）
  
- **文件管理**:
  - 文件上传下载
  - 文件列表查看
  - 文件删除
  - 文件重命名
  - 文件搜索
  - 自动存储空间检查
  
- **数据库集成**:
  - MySQL连接池
  - 事务支持
  - 自动重连

## 系统架构

```
cloudisk/
├── include/          # 头文件
│   ├── thread_pool.h
│   ├── database.h
│   ├── user_manager.h
│   ├── file_manager.h
│   ├── http_handler.h
│   ├── session_manager.h
│   ├── json_helper.h
│   └── server.h
├── src/             # 源文件
│   ├── main.cpp
│   ├── server.cpp
│   ├── thread_pool.cpp
│   ├── database.cpp
│   ├── user_manager.cpp
│   ├── file_manager.cpp
│   ├── http_handler.cpp
│   ├── session_manager.cpp
│   └── json_helper.cpp
├── CMakeLists.txt   # 构建配置
├── Dockerfile       # Docker镜像
├── docker-compose.yml
├── init.sql         # 数据库初始化
└── build.sh         # 编译部署脚本
```

## 快速开始

### 方式1: 本地编译部署（WSL/Linux）

#### 1. 安装依赖
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential cmake libmysqlclient-dev libssl-dev uuid-dev mysql-server

# 启动MySQL
sudo service mysql start
```

#### 2. 使用自动化脚本
```bash
chmod +x build.sh
./build.sh
# 选择选项 6 进行完整部署
```

#### 3. 手动编译（可选）
```bash
# 创建构建目录
mkdir build && cd build

# CMake配置
cmake ..

# 编译
make -j$(nproc)

# 返回项目根目录
cd ..
```

#### 4. 初始化数据库
```bash
mysql -u root -p < init.sql
# 输入MySQL密码
```

#### 5. 配置环境变量
```bash
# 创建.env文件或设置环境变量
export SERVER_PORT=8080
export STORAGE_PATH=./storage
export DB_HOST=localhost
export DB_USER=root
export DB_PASSWORD=your_password
export DB_NAME=cloudisk
```

#### 6. 运行服务器
```bash
./build/cloudisk_server
```

### 方式2: Docker部署

#### 1. 构建并启动
```bash
docker-compose up --build -d
```

#### 2. 查看日志
```bash
docker-compose logs -f cloudisk_server
```

#### 3. 停止服务
```bash
docker-compose down
```

## API接口文档

### 基础URL
```
http://localhost:8080
```

### 1. 用户注册
**POST** `/api/register`

请求体:
```json
{
  "username": "testuser",
  "email": "test@example.com",
  "password": "password123"
}
```

响应:
```json
{
  "success": true,
  "message": "User registered successfully"
}
```

### 2. 用户登录
**POST** `/api/login`

请求体:
```json
{
  "username": "testuser",
  "password": "password123"
}
```

响应:
```json
{
  "success": true,
  "message": "Login successful",
  "data": {
    "id": 1,
    "username": "testuser",
    "email": "test@example.com",
    "storage_used": 0,
    "storage_limit": 10737418240,
    "token": "abc123..."
  }
}
```

### 3. 获取用户信息
**GET** `/api/user/info`

请求头:
```
Authorization: Bearer {token}
```

响应:
```json
{
  "success": true,
  "message": "Success",
  "data": {
    "id": 1,
    "username": "testuser",
    "email": "test@example.com",
    "storage_used": 1048576,
    "storage_limit": 10737418240
  }
}
```

### 4. 文件上传
**POST** `/api/file/upload`

请求头:
```
Authorization: Bearer {token}
X-Filename: document.pdf
Content-Type: application/octet-stream
```

请求体: 二进制文件数据

响应:
```json
{
  "success": true,
  "message": "File uploaded successfully",
  "data": {
    "id": 1,
    "filename": "document.pdf",
    "size": 1048576
  }
}
```

### 5. 文件列表
**GET** `/api/file/list?offset=0&limit=10`

请求头:
```
Authorization: Bearer {token}
```

响应:
```json
{
  "success": true,
  "message": "Success",
  "data": [
    {
      "id": 1,
      "filename": "document.pdf",
      "size": 1048576,
      "mime_type": "application/pdf",
      "upload_date": "2024-01-01 10:00:00"
    }
  ]
}
```

### 6. 文件下载
**GET** `/api/file/download/{file_id}`

请求头:
```
Authorization: Bearer {token}
```

响应: 文件二进制数据

### 7. 文件删除
**DELETE** `/api/file/delete/{file_id}`

请求头:
```
Authorization: Bearer {token}
```

响应:
```json
{
  "success": true,
  "message": "File deleted successfully"
}
```

### 8. 文件重命名
**PUT** `/api/file/rename`

请求头:
```
Authorization: Bearer {token}
```

请求体:
```json
{
  "file_id": 1,
  "new_filename": "new_document.pdf"
}
```

响应:
```json
{
  "success": true,
  "message": "File renamed successfully"
}
```

### 9. 文件搜索
**GET** `/api/file/search?keyword=document`

请求头:
```
Authorization: Bearer {token}
```

响应:
```json
{
  "success": true,
  "message": "Success",
  "data": [...]
}
```

### 10. 用户登出
**POST** `/api/logout`

请求头:
```
Authorization: Bearer {token}
```

响应:
```json
{
  "success": true,
  "message": "Logout successful"
}
```

## 测试

### 使用curl测试

```bash
# 1. 注册用户
curl -X POST http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","email":"test@example.com","password":"test123"}'

# 2. 登录
TOKEN=$(curl -X POST http://localhost:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}' \
  | jq -r '.data.token')

# 3. 上传文件
curl -X POST http://localhost:8080/api/file/upload \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-Filename: test.txt" \
  --data-binary @test.txt

# 4. 获取文件列表
curl -X GET http://localhost:8080/api/file/list \
  -H "Authorization: Bearer $TOKEN"

# 5. 下载文件
curl -X GET http://localhost:8080/api/file/download/1 \
  -H "Authorization: Bearer $TOKEN" \
  -o downloaded_file.txt
```

## 部署到云服务器

### 1. 准备云服务器
- Ubuntu 20.04/22.04
- 2GB+ RAM
- 开放8080端口

### 2. 上传代码
```bash
# 使用scp或git
scp -r ./* user@server_ip:/path/to/cloudisk/
# 或
git clone your_repository.git
```

### 3. 安装依赖并编译
```bash
ssh user@server_ip
cd /path/to/cloudisk
./build.sh
# 选择选项 6
```

### 4. 使用systemd管理服务

创建服务文件:
```bash
sudo nano /etc/systemd/system/cloudisk.service
```

内容:
```ini
[Unit]
Description=Cloud Disk Server
After=network.target mysql.service

[Service]
Type=simple
User=your_user
WorkingDirectory=/path/to/cloudisk
Environment="SERVER_PORT=8080"
Environment="STORAGE_PATH=/path/to/cloudisk/storage"
Environment="DB_HOST=localhost"
Environment="DB_USER=root"
Environment="DB_PASSWORD=your_password"
Environment="DB_NAME=cloudisk"
ExecStart=/path/to/cloudisk/build/cloudisk_server
Restart=always

[Install]
WantedBy=multi-user.target
```

启动服务:
```bash
sudo systemctl daemon-reload
sudo systemctl enable cloudisk
sudo systemctl start cloudisk
sudo systemctl status cloudisk
```

### 5. 配置Nginx反向代理（可选）

```nginx
server {
    listen 80;
    server_name your_domain.com;
    
    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        client_max_body_size 100M;
    }
}
```

## 性能优化

1. **线程池大小**: 根据CPU核心数调整（默认10）
2. **数据库连接池**: 根据并发需求调整（默认10）
3. **会话超时**: 默认30分钟
4. **文件存储**: 使用SSD提升I/O性能

## 故障排除

### 编译错误
```bash
# 检查依赖是否安装
dpkg -l | grep -E 'libmysqlclient-dev|libssl-dev|uuid-dev'

# 清理重新编译
rm -rf build
./build.sh
```

### 数据库连接失败
```bash
# 检查MySQL状态
sudo service mysql status

# 检查密码和权限
mysql -u root -p

# 重新初始化数据库
mysql -u root -p < init.sql
```

### 端口被占用
```bash
# 查看端口占用
sudo lsof -i :8080

# 修改端口
export SERVER_PORT=8081
```

## 安全建议

1. **修改默认密码**: 更改init.sql中的测试用户密码
2. **使用HTTPS**: 在生产环境配置SSL证书
3. **防火墙配置**: 只开放必要端口
4. **定期备份**: 备份数据库和存储文件
5. **日志监控**: 设置日志记录和监控

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 联系方式

如有问题，请提交Issue。
