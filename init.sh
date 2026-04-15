#!/bin/bash

# CloudDisk 服务器初始化脚本
# 功能: 提供 10 个菜单选项用于服务器初始化、配置、维护

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warn() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

log_blue() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

# 获取项目目录
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$PROJECT_DIR/.env"

# 检查系统依赖
check_system_deps() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  检查系统依赖${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    # 检查 Docker
    if command -v docker &> /dev/null; then
        DOCKER_VERSION=$(docker --version)
        log_info "Docker: $DOCKER_VERSION"
    else
        log_error "Docker 未安装!"
        return 1
    fi
    
    # 检查 Docker Compose
    if command -v docker-compose &> /dev/null; then
        COMPOSE_VERSION=$(docker-compose --version)
        log_info "Docker Compose: $COMPOSE_VERSION"
    else
        log_error "Docker Compose 未安装!"
        return 1
    fi
    
    # 检查 Git
    if command -v git &> /dev/null; then
        GIT_VERSION=$(git --version)
        log_info "Git: $GIT_VERSION"
    else
        log_warn "Git 未安装 (可选)"
    fi
    
    # 检查项目目录
    if [ -d "$PROJECT_DIR" ]; then
        log_info "项目目录存在: $PROJECT_DIR"
    else
        log_error "项目目录不存在: $PROJECT_DIR"
        return 1
    fi
    
    # 检查关键文件
    if [ -f "$PROJECT_DIR/docker-compose.yml" ]; then
        log_info "docker-compose.yml 存在"
    else
        log_error "docker-compose.yml 不存在"
        return 1
    fi
    
    echo
    log_info "系统依赖检查完成! ✨"
    echo
    read -p "按 Enter 继续..."
}

# 生成环境配置
generate_env() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  生成环境配置${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    if [ -f "$ENV_FILE" ]; then
        log_warn "警告: $ENV_FILE 已存在"
        read -p "是否覆盖? (y/n) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            return 0
        fi
        cp "$ENV_FILE" "$ENV_FILE.backup"
        log_info "备份已保存: $ENV_FILE.backup"
    fi
    
    # 生成强密码
    MYSQL_ROOT_PASSWORD=$(openssl rand -base64 16)
    MYSQL_PASSWORD=$(openssl rand -base64 16)
    
    cat > "$ENV_FILE" << EOF
# MySQL 配置
MYSQL_ROOT_PASSWORD=$MYSQL_ROOT_PASSWORD
MYSQL_DATABASE=cloudisk
MYSQL_USER=cloudisk_user
MYSQL_PASSWORD=$MYSQL_PASSWORD

# 服务器配置
SERVER_PORT=8080
STORAGE_PATH=/data/storage
LOG_LEVEL=INFO
EOF
    
    log_info ".env 文件已生成"
    log_info "MySQL root 密码: $MYSQL_ROOT_PASSWORD"
    log_info "MySQL 用户密码: $MYSQL_PASSWORD"
    echo
    echo "⚠️  请妥善保管上述密码!"
    echo
    read -p "按 Enter 继续..."
}

# 初始化 Docker 容器
init_docker_containers() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  初始化 Docker 容器${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    cd "$PROJECT_DIR"
    
    # 停止现有容器
    log_info "停止现有容器..."
    docker-compose down || true
    
    # 构建镜像
    log_info "构建 Docker 镜像..."
    docker-compose build
    
    # 启动容器
    log_info "启动 Docker 容器..."
    docker-compose up -d
    
    # 等待 MySQL 启动
    log_info "等待 MySQL 启动..."
    sleep 10
    
    # 验证容器状态
    log_info "验证容器状态..."
    docker-compose ps
    
    echo
    log_info "Docker 容器初始化完成! 🐳"
    echo
    read -p "按 Enter 继续..."
}

# 验证数据库
verify_database() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  验证数据库${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    cd "$PROJECT_DIR"
    
    log_info "测试 MySQL 连接..."
    docker-compose exec -T mysql mysql -uroot -p$(grep MYSQL_ROOT_PASSWORD .env | cut -d= -f2) -e "SELECT 1" > /dev/null && log_info "MySQL 连接成功!" || log_error "MySQL 连接失败"
    
    log_info "检查数据表..."
    TABLE_COUNT=$(docker-compose exec -T mysql mysql -uroot -p$(grep MYSQL_ROOT_PASSWORD .env | cut -d= -f2) cloudisk -e "SHOW TABLES" | wc -l)
    log_info "数据表数量: $((TABLE_COUNT - 1))"
    
    echo
    log_info "数据库验证完成! 📊"
    echo
    read -p "按 Enter 继续..."
}

# 运行健康检查
run_health_check() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  运行健康检查${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    cd "$PROJECT_DIR"
    
    log_info "显示应用日志 (最后 20 行)..."
    docker-compose logs --tail 20 cloudisk_app || docker-compose logs --tail 20 | head -20
    
    log_info "测试前端页面..."
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/ || echo "000")
    if [ "$HTTP_CODE" = "200" ]; then
        log_info "前端页面响应正常 (HTTP $HTTP_CODE)"
    else
        log_warn "前端页面可能异常 (HTTP $HTTP_CODE)"
    fi
    
    echo
    log_info "健康检查完成! 🏥"
    echo
    read -p "按 Enter 继续..."
}

# 完整初始化流程
full_initialization() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}   完整初始化流程${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    log_info "开始完整初始化流程..."
    echo
    
    # 1. 检查依赖
    log_blue "[1/5] 检查系统依赖..."
    if ! check_system_deps > /dev/null 2>&1; then
        log_error "系统依赖检查失败"
        return 1
    fi
    log_info "系统依赖检查完成"
    
    # 2. 生成配置
    log_blue "[2/5] 生成环境配置..."
    generate_env > /dev/null 2>&1
    log_info "环境配置生成完成"
    
    # 3. 初始化容器
    log_blue "[3/5] 初始化 Docker 容器..."
    init_docker_containers > /dev/null 2>&1
    log_info "Docker 容器初始化完成"
    
    # 4. 验证数据库
    log_blue "[4/5] 验证数据库..."
    verify_database > /dev/null 2>&1
    log_info "数据库验证完成"
    
    # 5. 健康检查
    log_blue "[5/5] 运行健康检查..."
    run_health_check > /dev/null 2>&1
    log_info "健康检查完成"
    
    echo
    echo -e "${GREEN}🎉 初始化完成!${NC}"
    echo
    echo "访问应用: http://localhost:8080"
    echo
    read -p "按 Enter 返回菜单..."
}

# 快速启动
quick_start() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  快速启动${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    cd "$PROJECT_DIR"
    
    log_info "启动 Docker 容器..."
    docker-compose up -d
    
    log_info "等待服务启动..."
    sleep 5
    
    log_info "显示容器状态..."
    docker-compose ps
    
    echo
    log_info "快速启动完成!"
    echo
    read -p "按 Enter 返回菜单..."
}

# 停止服务
stop_service() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  停止服务${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    cd "$PROJECT_DIR"
    
    read -p "确认停止所有容器? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log_info "停止容器..."
        docker-compose down
        log_info "容器已停止"
    else
        log_info "已取消"
    fi
    
    echo
    read -p "按 Enter 返回菜单..."
}

# 查看日志
view_logs() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  查看日志${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    log_blue "按 Ctrl+C 退出日志查看"
    echo
    
    cd "$PROJECT_DIR"
    docker-compose logs -f
}

# 清理数据
cleanup() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}   清理所有数据${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    
    log_error "⚠️  这将删除所有容器、数据库和副本文件!"
    read -p "确认删除所有数据? (yes/no) " response
    
    if [ "$response" = "yes" ]; then
        cd "$PROJECT_DIR"
        log_info "删除容器..."
        docker-compose down -v
        log_info "清理完成"
    else
        log_info "已取消"
    fi
    
    echo
    read -p "按 Enter 返回菜单..."
}

# 主菜单
show_menu() {
    clear
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  CloudDisk 初始化菜单${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
    echo "1. 检查系统依赖"
    echo "2. 生成环境配置"
    echo "3. 初始化 Docker 容器"
    echo "4. 验证数据库"
    echo "5. 运行健康检查"
    echo "6. 完整初始化流程 ✨ 【推荐】"
    echo "7. 快速启动"
    echo "8. 停止服务"
    echo "9. 查看日志"
    echo "10. 清理所有数据"
    echo "0. 退出"
    echo
}

# 主循环
main_loop() {
    while true; do
        show_menu
        read -p "请选择操作 [0-10]: " choice
        
        case $choice in
            1) check_system_deps ;;
            2) generate_env ;;
            3) init_docker_containers ;;
            4) verify_database ;;
            5) run_health_check ;;
            6) full_initialization ;;
            7) quick_start ;;
            8) stop_service ;;
            9) view_logs ;;
            10) cleanup ;;
            0) log_info "再见!"; exit 0 ;;
            *) log_error "无效的选择"; sleep 1 ;;
        esac
    done
}

# 检查是否以 sudo 运行
if [ "$EUID" -ne 0 ]; then
    echo "请使用 sudo 运行此脚本"
    exit 1
fi

# 运行主循环
main_loop
