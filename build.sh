#!/bin/bash

# lightweight_comm_server 编译和部署脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  lightweight_comm_server - 编译和部署脚本${NC}"
echo -e "${GREEN}========================================${NC}"

# 检查依赖
check_dependencies() {
    echo -e "${YELLOW}检查依赖...${NC}"
    
    dependencies=("cmake" "g++" "mysql")
    for dep in "${dependencies[@]}"; do
        if ! command -v $dep &> /dev/null; then
            echo -e "${RED}错误: $dep 未安装${NC}"
            exit 1
        fi
    done
    
    echo -e "${GREEN}✓ 依赖检查完成${NC}"
}

# 安装依赖库
install_libraries() {
    echo -e "${YELLOW}安装依赖库...${NC}"
    
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            libmysqlclient-dev \
            libssl-dev \
            uuid-dev
    else
        echo -e "${YELLOW}请手动安装所需的依赖库${NC}"
    fi
    
    echo -e "${GREEN}✓ 依赖库安装完成${NC}"
}

# 编译项目
compile_project() {
    echo -e "${YELLOW}开始编译...${NC}"
    
    # 创建构建目录
    if [ -d "build" ]; then
        rm -rf build
    fi
    mkdir build
    cd build
    
    # CMake配置
    cmake ..
    
    # 编译
    make -j$(nproc)
    
    cd ..
    
    echo -e "${GREEN}✓ 编译完成${NC}"
}

# 初始化数据库
init_database() {
    echo -e "${YELLOW}初始化数据库...${NC}"
    
    read -p "MySQL host [localhost]: " DB_HOST
    DB_HOST=${DB_HOST:-localhost}
    
    read -p "MySQL user [root]: " DB_USER
    DB_USER=${DB_USER:-root}
    
    read -sp "MySQL password: " DB_PASS
    echo
    
    mysql -h "$DB_HOST" -u "$DB_USER" -p"$DB_PASS" < init.sql
    
    echo -e "${GREEN}✓ 数据库初始化完成${NC}"
}

# 创建配置文件
create_config() {
    echo -e "${YELLOW}创建配置文件...${NC}"
    
    cat > .env << EOF
# 服务器配置
SERVER_PORT=8080
STORAGE_PATH=./storage

# 数据库配置
DB_HOST=localhost
DB_USER=root
DB_PASSWORD=your_password
DB_NAME=cloudisk
EOF
    
    echo -e "${GREEN}✓ 配置文件创建完成${NC}"
    echo -e "${YELLOW}请编辑 .env 文件设置你的数据库密码${NC}"
}

# 创建存储目录
create_storage() {
    echo -e "${YELLOW}创建存储目录...${NC}"
    
    mkdir -p storage
    chmod 755 storage
    
    echo -e "${GREEN}✓ 存储目录创建完成${NC}"
}

# 运行服务器
run_server() {
    echo -e "${YELLOW}启动服务器...${NC}"
    
    # 加载环境变量
    if [ -f ".env" ]; then
        export $(cat .env | xargs)
    fi
    
    ./build/lightweight_comm_server
}

# 主菜单
show_menu() {
    echo ""
    echo "请选择操作:"
    echo "1. 安装依赖"
    echo "2. 编译项目"
    echo "3. 初始化数据库"
    echo "4. 创建配置文件"
    echo "5. 运行服务器"
    echo "6. 完整部署 (1-5步骤)"
    echo "7. Docker部署"
    echo "0. 退出"
    echo ""
    read -p "请输入选项 [0-7]: " choice
    
    case $choice in
        1)
            install_libraries
            ;;
        2)
            compile_project
            ;;
        3)
            init_database
            ;;
        4)
            create_config
            create_storage
            ;;
        5)
            run_server
            ;;
        6)
            install_libraries
            compile_project
            init_database
            create_config
            create_storage
            echo -e "${GREEN}========================================${NC}"
            echo -e "${GREEN}部署完成！${NC}"
            echo -e "${GREEN}========================================${NC}"
            echo -e "运行 './build.sh' 并选择选项5 启动服务器"
            ;;
        7)
            echo -e "${YELLOW}使用Docker Compose部署...${NC}"
            docker-compose up --build -d
            echo -e "${GREEN}✓ Docker部署完成${NC}"
            echo -e "服务器运行在: http://localhost:8080"
            ;;
        0)
            echo "退出"
            exit 0
            ;;
        *)
            echo -e "${RED}无效选项${NC}"
            ;;
    esac
    
    show_menu
}

# 主程序
main() {
    check_dependencies
    show_menu
}

main
