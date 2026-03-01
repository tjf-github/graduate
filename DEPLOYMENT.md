# 云服务器部署指南

## 准备工作

### 1. 云服务器要求
- **操作系统**: Ubuntu 20.04 或 22.04 LTS
- **配置**: 至少 2GB RAM, 2核CPU, 20GB磁盘
- **网络**: 公网IP，开放端口 80/443/8080

### 2. 域名准备（可选）
- 如果需要使用域名访问，准备一个域名并解析到服务器IP

## 部署步骤

### 步骤1: 连接到服务器

```bash
ssh root@your_server_ip
# 或使用密钥
ssh -i your_key.pem ubuntu@your_server_ip
```

### 步骤2: 更新系统

```bash
sudo apt-get update
sudo apt-get upgrade -y
```

### 步骤3: 安装依赖

```bash
# 安装编译工具
sudo apt-get install -y build-essential cmake git

# 安装MySQL
sudo apt-get install -y mysql-server mysql-client libmysqlclient-dev

# 安装其他依赖
sudo apt-get install -y libssl-dev uuid-dev

# 安装可选工具
sudo apt-get install -y nginx certbot python3-certbot-nginx
```

### 步骤4: 配置MySQL

```bash
# 启动MySQL
sudo systemctl start mysql
sudo systemctl enable mysql

# 安全配置
sudo mysql_secure_installation
# 按提示设置root密码和安全选项

# 登录MySQL
sudo mysql -u root -p

# 执行以下SQL命令:
```

```sql
-- 创建数据库用户（推荐不使用root）
CREATE USER 'cloudisk'@'localhost' IDENTIFIED BY 'strong_password_here';
GRANT ALL PRIVILEGES ON cloudisk.* TO 'cloudisk'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

### 步骤5: 上传代码

**方法1: 使用Git**
```bash
cd /opt
sudo git clone https://github.com/yourusername/cloudisk.git
cd cloudisk
```

**方法2: 使用SCP**
```bash
# 在本地执行
scp -r /path/to/cloudisk root@your_server_ip:/opt/
```

**方法3: 使用rsync**
```bash
# 在本地执行
rsync -avz /path/to/cloudisk/ root@your_server_ip:/opt/cloudisk/
```

### 步骤6: 编译项目

```bash
cd /opt/cloudisk

# 运行编译脚本
chmod +x build.sh
./build.sh

# 选择选项2进行编译
# 或手动编译:
mkdir build && cd build
cmake ..
make -j$(nproc)
cd ..
```

### 步骤7: 初始化数据库

```bash
# 使用创建的数据库用户
mysql -u cloudisk -p cloudisk < init.sql

# 或使用root用户
sudo mysql -u root -p < init.sql
```

### 步骤8: 配置环境

```bash
# 创建配置文件
sudo nano /opt/cloudisk/.env
```

添加以下内容（根据实际情况修改）:
```bash
SERVER_PORT=8080
STORAGE_PATH=/opt/cloudisk/storage
DB_HOST=localhost
DB_USER=cloudisk
DB_PASSWORD=your_strong_password
DB_NAME=cloudisk
```

```bash
# 创建存储目录
sudo mkdir -p /opt/cloudisk/storage
sudo chmod 755 /opt/cloudisk/storage
```

### 步骤9: 配置Systemd服务

```bash
sudo nano /etc/systemd/system/cloudisk.service
```

添加以下内容:
```ini
[Unit]
Description=Cloud Disk Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
User=root
WorkingDirectory=/opt/cloudisk
EnvironmentFile=/opt/cloudisk/.env
ExecStart=/opt/cloudisk/build/cloudisk_server
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

启动服务:
```bash
# 重载systemd
sudo systemctl daemon-reload

# 启用开机自启
sudo systemctl enable cloudisk

# 启动服务
sudo systemctl start cloudisk

# 查看状态
sudo systemctl status cloudisk

# 查看日志
sudo journalctl -u cloudisk -f
```

### 步骤10: 配置防火墙

```bash
# 如果使用UFW
sudo ufw allow 8080/tcp
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw allow OpenSSH
sudo ufw enable

# 查看状态
sudo ufw status
```

### 步骤11: 配置Nginx反向代理（可选但推荐）

```bash
sudo nano /etc/nginx/sites-available/cloudisk
```

添加以下内容:
```nginx
server {
    listen 80;
    server_name your_domain.com;  # 修改为你的域名
    
    # 最大上传大小
    client_max_body_size 100M;
    
    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # 超时设置
        proxy_connect_timeout 300;
        proxy_send_timeout 300;
        proxy_read_timeout 300;
    }
}
```

启用配置:
```bash
sudo ln -s /etc/nginx/sites-available/cloudisk /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

### 步骤12: 配置SSL证书（可选但推荐）

使用Let's Encrypt免费证书:
```bash
sudo certbot --nginx -d your_domain.com
```

按提示完成配置，Certbot会自动修改Nginx配置并配置自动续期。

### 步骤13: 测试部署

```bash
# 测试服务器响应
curl http://localhost:8080/api/user/info

# 如果配置了Nginx
curl http://your_domain.com/api/user/info

# 从外部测试
curl http://your_server_ip:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","email":"test@test.com","password":"test123"}'
```

## Docker部署方式（替代方案）

### 方法1: 使用Docker Compose

```bash
# 安装Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# 安装Docker Compose
sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

# 部署
cd /opt/cloudisk
sudo docker-compose up -d

# 查看日志
sudo docker-compose logs -f

# 停止服务
sudo docker-compose down
```

### 方法2: 手动Docker部署

```bash
# 构建镜像
sudo docker build -t cloudisk-server .

# 运行MySQL
sudo docker run -d \
  --name cloudisk-mysql \
  -e MYSQL_ROOT_PASSWORD=your_password \
  -e MYSQL_DATABASE=cloudisk \
  -v mysql_data:/var/lib/mysql \
  -p 3306:3306 \
  mysql:8.0

# 等待MySQL启动
sleep 30

# 初始化数据库
sudo docker exec -i cloudisk-mysql mysql -uroot -pyour_password < init.sql

# 运行服务器
sudo docker run -d \
  --name cloudisk-server \
  --link cloudisk-mysql:mysql \
  -e SERVER_PORT=8080 \
  -e DB_HOST=mysql \
  -e DB_USER=root \
  -e DB_PASSWORD=your_password \
  -e DB_NAME=cloudisk \
  -v storage_data:/app/storage \
  -p 8080:8080 \
  cloudisk-server
```

## 维护和管理

### 日志查看

```bash
# Systemd服务日志
sudo journalctl -u cloudisk -f

# 或查看最近100行
sudo journalctl -u cloudisk -n 100

# Docker日志
sudo docker-compose logs -f cloudisk_server
```

### 服务管理

```bash
# 停止服务
sudo systemctl stop cloudisk

# 启动服务
sudo systemctl start cloudisk

# 重启服务
sudo systemctl restart cloudisk

# 查看状态
sudo systemctl status cloudisk
```

### 数据库备份

```bash
# 创建备份脚本
sudo nano /opt/cloudisk/backup.sh
```

```bash
#!/bin/bash
BACKUP_DIR="/opt/backups"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# 备份数据库
mysqldump -u cloudisk -p'your_password' cloudisk > $BACKUP_DIR/cloudisk_$DATE.sql

# 备份存储文件
tar -czf $BACKUP_DIR/storage_$DATE.tar.gz /opt/cloudisk/storage

# 删除30天前的备份
find $BACKUP_DIR -name "*.sql" -mtime +30 -delete
find $BACKUP_DIR -name "*.tar.gz" -mtime +30 -delete

echo "Backup completed: $DATE"
```

```bash
# 添加执行权限
sudo chmod +x /opt/cloudisk/backup.sh

# 添加到crontab (每天凌晨2点备份)
sudo crontab -e
# 添加: 0 2 * * * /opt/cloudisk/backup.sh >> /var/log/cloudisk-backup.log 2>&1
```

### 更新部署

```bash
# 拉取最新代码
cd /opt/cloudisk
sudo git pull

# 重新编译
cd build
sudo make clean
sudo cmake ..
sudo make -j$(nproc)

# 重启服务
sudo systemctl restart cloudisk
```

### 监控

安装监控工具:
```bash
# 安装htop
sudo apt-get install htop

# 监控系统资源
htop

# 监控磁盘使用
df -h

# 监控数据库连接
sudo mysql -u root -p -e "SHOW PROCESSLIST;"

# 监控服务器端口
sudo netstat -tlnp | grep 8080
```

## 故障排除

### 1. 服务启动失败

```bash
# 查看详细日志
sudo journalctl -u cloudisk -xe

# 检查端口是否被占用
sudo lsof -i :8080

# 手动运行查看错误
cd /opt/cloudisk
./build/cloudisk_server
```

### 2. 数据库连接失败

```bash
# 测试数据库连接
mysql -u cloudisk -p -h localhost cloudisk

# 检查MySQL状态
sudo systemctl status mysql

# 查看MySQL错误日志
sudo tail -f /var/log/mysql/error.log
```

### 3. 文件上传失败

```bash
# 检查存储目录权限
ls -la /opt/cloudisk/storage

# 修复权限
sudo chmod 755 /opt/cloudisk/storage
sudo chown -R root:root /opt/cloudisk/storage
```

### 4. 内存不足

```bash
# 查看内存使用
free -h

# 配置swap
sudo fallocate -l 2G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

## 安全加固

### 1. 修改SSH端口

```bash
sudo nano /etc/ssh/sshd_config
# 修改 Port 22 为其他端口，如 2222
sudo systemctl restart sshd
```

### 2. 配置fail2ban

```bash
sudo apt-get install fail2ban
sudo systemctl enable fail2ban
sudo systemctl start fail2ban
```

### 3. 定期更新系统

```bash
sudo apt-get update
sudo apt-get upgrade -y
```

### 4. 限制数据库远程访问

```bash
sudo nano /etc/mysql/mysql.conf.d/mysqld.cnf
# 确保 bind-address = 127.0.0.1
sudo systemctl restart mysql
```

## 性能优化

### 1. 调整线程池大小

编辑 `src/server.cpp`:
```cpp
// 根据CPU核心数调整
thread_pool = std::make_unique<ThreadPool>(20); // 增加到20
```

### 2. 优化MySQL

```bash
sudo nano /etc/mysql/mysql.conf.d/mysqld.cnf
```

添加:
```ini
[mysqld]
max_connections = 200
innodb_buffer_pool_size = 512M
query_cache_size = 64M
```

### 3. 使用SSD存储

将storage目录挂载到SSD:
```bash
sudo mount /dev/sdb1 /opt/cloudisk/storage
```

## 扩展功能

### 1. 添加文件分享功能

已在数据库中预留share_links表

### 2. 添加文件夹功能

已在数据库中预留folders表

### 3. 添加文件预览

可以集成文件预览服务

### 4. 添加文件秒传

可以基于文件hash实现秒传功能

## 总结

完成以上步骤后，你的云盘系统应该已经成功部署并运行在云服务器上了。

**访问地址**:
- 直接访问: http://your_server_ip:8080
- 通过Nginx: http://your_domain.com
- HTTPS: https://your_domain.com

如有问题，请查看日志文件并参考故障排除部分。
