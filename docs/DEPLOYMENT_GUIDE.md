# 云服务器部署指南

## 1. 环境要求

- Ubuntu 20.04 或 22.04 LTS
- 至少 2GB RAM、2 核 CPU、20GB 磁盘
- 开放 `80/443/8080` 端口

## 2. 安装依赖

```bash
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install -y build-essential cmake git
sudo apt-get install -y mysql-server mysql-client libmysqlclient-dev
sudo apt-get install -y libssl-dev uuid-dev
sudo apt-get install -y nginx certbot python3-certbot-nginx
```

## 3. 数据库准备

```bash
sudo systemctl start mysql
sudo systemctl enable mysql
sudo mysql_secure_installation
```

创建业务库和用户后，执行：

```bash
mysql -u cloudisk -p cloudisk < init.sql
```

## 4. 代码部署

可选择：

- `git clone`
- `scp`
- `rsync`

部署目录建议统一为：

```text
/opt/cloudisk
```

## 5. 环境变量

建议在 `/opt/cloudisk/.env` 中维护：

```bash
SERVER_PORT=8080
STORAGE_PATH=/opt/cloudisk/storage
STATIC_DIR=/opt/cloudisk/static
DB_HOST=localhost
DB_PORT=3306
DB_USER=cloudisk
DB_PASSWORD=your_strong_password
DB_NAME=cloudisk
```

同时创建存储目录：

```bash
sudo mkdir -p /opt/cloudisk/storage
sudo chmod 755 /opt/cloudisk/storage
```

## 6. 编译

```bash
cd /opt/cloudisk
cmake -S . -B build
cmake --build build
```

## 7. Systemd

仓库已提供模板：

- [../deploy/cloudisk.service](../deploy/cloudisk.service)

典型配置：

```ini
[Unit]
Description=Lightweight Comm Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
WorkingDirectory=/opt/cloudisk
EnvironmentFile=/opt/cloudisk/.env
ExecStart=/opt/cloudisk/build/lightweight_comm_server
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

启用服务：

```bash
sudo systemctl daemon-reload
sudo systemctl enable cloudisk
sudo systemctl start cloudisk
sudo systemctl status cloudisk
```

## 8. Nginx 反向代理

示例：

```nginx
server {
    listen 80;
    server_name your_domain.com;

    client_max_body_size 100M;

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_connect_timeout 60s;
        proxy_send_timeout 300s;
        proxy_read_timeout 300s;
    }
}
```

## 9. SSL

```bash
sudo certbot --nginx -d your_domain.com
```

## 10. 验证

```bash
curl -I http://localhost:8080/
curl http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","email":"test@test.com","password":"test123"}'
```

## 11. 补充说明

如果准备启用本次大文件上传能力，还需要同步确认：

- `init.sql` 已包含上传会话相关表
- `CMakeLists.txt` 已正确链接 OpenSSL
- Nginx 的 `client_max_body_size` 与业务预期一致

## 12. 部署摘要

最短执行路径：

1. 安装依赖
2. 初始化 MySQL 和 `init.sql`
3. 配置 `.env`
4. 编译项目
5. 启动 `cloudisk.service`
6. 访问 `http://<server>:8080/`

最小验收项：

- 首页可访问
- 可以注册和登录
- 可以上传普通文件
- 文件列表正常刷新
- 分享下载可用
- 消息接口可用

如果本次大文件能力已补完，还应额外验证：

- 初始化上传
- 分块上传与进度查询
- 完成上传后文件可下载

## 13. init.sh 菜单说明

`init.sh` 用于服务器上的快速初始化和服务管理，常用项如下：

### 1. 检查系统依赖

检查 Docker、Docker Compose、Git、项目目录和仓库初始化情况。

### 6. 完整初始化流程

推荐首次部署使用，会依次执行：

1. 检查依赖
2. 生成环境配置
3. 初始化 Docker 容器
4. 验证数据库
5. 运行健康检查

### 7. 快速启动

适合服务器重启后恢复服务，或容器停止后的快速拉起。

### 8. 停止服务

停止容器，但保留数据库数据和上传文件。

### 9. 查看日志

用于实时观察运行状态，退出方式为 `Ctrl+C`。

### 10. 清理所有数据

高风险且不可逆，只适合完全重置环境时使用。
