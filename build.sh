#!/usr/bin/env bash

# lightweight_comm_server 构建与运行辅助脚本（Linux/WSL2 优先）

set -Eeuo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BINARY_PATH="${BUILD_DIR}/lightweight_comm_server"
CPU_COUNT="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
HEALTH_CHECK_CANDIDATES=(
    "${PROJECT_DIR}/scripts/project_health_check.sh"
    "${PROJECT_DIR}/tools/project_health_check.sh"
    "${PROJECT_DIR}/project_health_check.sh"
)
LOGIC_TEST_CANDIDATES=(
    "${PROJECT_DIR}/scripts/run_logic_tests.sh"
    "${PROJECT_DIR}/tools/run_logic_tests.sh"
    "${PROJECT_DIR}/run_logic_tests.sh"
    "${PROJECT_DIR}/tests/functional_test.sh"
)

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok() { echo -e "${GREEN}[ OK ]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_err() { echo -e "${RED}[FAIL]${NC} $*"; }

find_existing_path() {
    local p
    for p in "$@"; do
        if [[ -e "$p" ]]; then
            echo "$p"
            return 0
        fi
    done
    return 1
}

is_port_in_use() {
    local port="$1"
    if command -v lsof >/dev/null 2>&1; then
        lsof -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1
        return $?
    fi
    if command -v netstat >/dev/null 2>&1; then
        netstat -ltn 2>/dev/null | awk '{print $4}' | grep -Eq "(^|:)$port$"
        return $?
    fi
    return 1
}

print_banner() {
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  lightweight_comm_server 构建脚本${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo "项目目录: ${PROJECT_DIR}"
    echo "构建目录: ${BUILD_DIR}"
}

on_error() {
    local line="$1"
    log_err "脚本在第 ${line} 行执行失败。"
    log_warn "可先执行: ./build.sh check"
}
trap 'on_error $LINENO' ERR

check_cmd() {
    local cmd="$1"
    if command -v "$cmd" >/dev/null 2>&1; then
        log_ok "已找到命令: $cmd"
        return 0
    fi
    log_err "缺少命令: $cmd"
    return 1
}

check_dependencies() {
    print_banner
    log_info "检查构建依赖..."

    local failed=0
    check_cmd cmake || failed=1
    check_cmd g++ || failed=1
    check_cmd make || failed=1

    # mysql 客户端不是编译必须，但数据库初始化会用到
    if command -v mysql >/dev/null 2>&1; then
        log_ok "已找到命令: mysql"
    else
        log_warn "未找到 mysql 客户端，'init-db' 步骤将不可用"
    fi

    # Linux/WSL2 常见头文件检查
    if [[ -f /usr/include/mysql/mysql.h ]]; then
        log_ok "MySQL 头文件存在: /usr/include/mysql/mysql.h"
    else
        log_warn "未发现 /usr/include/mysql/mysql.h（可能导致编译失败）"
        log_warn "Ubuntu/WSL2 可安装: sudo apt-get install -y libmysqlclient-dev"
    fi

    if [[ -e /usr/lib/x86_64-linux-gnu/libmysqlclient.so ]]; then
        log_ok "MySQL 客户端库存在: /usr/lib/x86_64-linux-gnu/libmysqlclient.so"
    else
        log_warn "未发现 libmysqlclient.so（可能导致链接失败）"
        log_warn "Ubuntu/WSL2 可安装: sudo apt-get install -y libmysqlclient-dev"
    fi

    if [[ "$failed" -ne 0 ]]; then
        log_err "依赖检查未通过，请先安装缺失命令后重试。"
        return 1
    fi

    if [[ -f "${PROJECT_DIR}/CMakeLists.txt" ]]; then
        log_ok "已找到 CMakeLists.txt"
    else
        log_err "未找到 CMakeLists.txt，请确认在项目根目录执行脚本"
        return 1
    fi

    if [[ -f "${PROJECT_DIR}/init.sql" ]]; then
        log_ok "已找到 init.sql"
    else
        log_warn "未找到 init.sql（init-db 步骤将不可用）"
    fi

    log_ok "依赖检查完成"
}

install_libraries() {
    print_banner
    if [[ "${OSTYPE:-}" == linux-gnu* ]]; then
        log_info "安装 Linux/WSL2 依赖（需要 sudo 权限）..."
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            libmysqlclient-dev \
            libssl-dev \
            uuid-dev
        log_ok "依赖安装完成"
    else
        log_warn "当前系统不是 Linux，未自动安装依赖。"
        log_warn "请手动安装: cmake g++ make libmysqlclient 开发库 libssl uuid"
    fi
}

configure_project() {
    log_info "执行 CMake 配置..."
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}"
    log_ok "CMake 配置成功"
}

build_project() {
    print_banner
    check_dependencies
    configure_project
    log_info "开始编译（并行任务: ${CPU_COUNT}）..."
    cmake --build "${BUILD_DIR}" -j"${CPU_COUNT}"
    log_ok "编译完成"

    if [[ -x "${BINARY_PATH}" ]]; then
        log_ok "可执行文件: ${BINARY_PATH}"
    else
        log_err "未找到构建产物: ${BINARY_PATH}"
        return 1
    fi
}

init_database() {
    print_banner

    if ! command -v mysql >/dev/null 2>&1; then
        log_err "未安装 mysql 客户端，无法初始化数据库。"
        log_warn "安装示例: sudo apt-get install -y mysql-client"
        return 1
    fi

    if [[ ! -f "${PROJECT_DIR}/init.sql" ]]; then
        log_err "未找到 init.sql"
        return 1
    fi

    local db_host db_port db_user db_pass
    read -r -p "MySQL host [127.0.0.1]: " db_host
    db_host="${db_host:-127.0.0.1}"

    read -r -p "MySQL port [3306]: " db_port
    db_port="${db_port:-3306}"

    read -r -p "MySQL user [root]: " db_user
    db_user="${db_user:-root}"

    read -r -s -p "MySQL password: " db_pass
    echo

    log_info "执行数据库初始化脚本 init.sql ..."
    mysql -h "${db_host}" -P "${db_port}" -u "${db_user}" -p"${db_pass}" < "${PROJECT_DIR}/init.sql"
    log_ok "数据库初始化完成"
}

create_config() {
    print_banner

    local env_file="${PROJECT_DIR}/.env"
    if [[ -f "${env_file}" ]]; then
        log_warn ".env 已存在: ${env_file}"
        local overwrite=""
        if [[ -t 0 ]]; then
            read -r -p "是否覆盖? [y/N]: " overwrite || true
        else
            log_info "检测到非交互环境，默认不覆盖现有 .env"
        fi
        if [[ ! "${overwrite:-}" =~ ^[Yy]$ ]]; then
            log_info "已跳过 .env 生成"
            return 0
        fi
    fi

    if [[ -f "${PROJECT_DIR}/.env.example" ]]; then
        cp "${PROJECT_DIR}/.env.example" "${env_file}"
        log_ok "已根据 .env.example 创建 .env"
    else
        cat > "${env_file}" <<'ENVEOF'
SERVER_PORT=8080
STORAGE_PATH=./storage
STATIC_DIR=./static

DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=cloudisk
DB_PASSWORD=change_me
DB_NAME=cloudisk
ENVEOF
        log_ok "已创建默认 .env"
    fi

    log_warn "请确认 .env 中 DB_USER/DB_PASSWORD/DB_HOST 与实际数据库一致"
}

create_storage() {
    print_banner
    mkdir -p "${PROJECT_DIR}/storage"
    chmod 755 "${PROJECT_DIR}/storage"
    log_ok "存储目录已就绪: ${PROJECT_DIR}/storage"
}

run_server() {
    print_banner

    if [[ ! -x "${BINARY_PATH}" ]]; then
        log_warn "未检测到可执行文件，先执行编译。"
        build_project
    fi

    if [[ -f "${PROJECT_DIR}/.env" ]]; then
        log_info "加载环境变量: .env"
        set -a
        # shellcheck disable=SC1091
        source "${PROJECT_DIR}/.env"
        set +a
    else
        log_warn "未找到 .env，将使用程序内默认配置。"
    fi

    local server_port="${SERVER_PORT:-8080}"
    local db_host="${DB_HOST:-127.0.0.1}"
    local db_port="${DB_PORT:-3306}"
    local db_user="${DB_USER:-cloudisk}"
    local db_password="${DB_PASSWORD:-}"
    local db_name="${DB_NAME:-cloudisk}"

    if [[ -z "${db_password}" || "${db_password}" == "change_me" ]]; then
        log_err "数据库密码未配置正确（当前为默认值 change_me 或空）。"
        log_warn "请先修改 .env：DB_PASSWORD=<你的真实密码>"
        log_warn "也请确认 DB_USER=${db_user}、DB_HOST=${db_host}、DB_PORT=${db_port}、DB_NAME=${db_name}"
        return 1
    fi

    if command -v mysqladmin >/dev/null 2>&1; then
        if mysqladmin ping -h"${db_host}" -P"${db_port}" -u"${db_user}" -p"${db_password}" --silent >/dev/null 2>&1; then
            log_ok "MySQL 连接预检通过: ${db_user}@${db_host}:${db_port}"
        else
            log_err "MySQL 连接预检失败: ${db_user}@${db_host}:${db_port}"
            log_warn "请检查用户名/密码授权，或先执行: ./build.sh init-db"
            return 1
        fi
    else
        log_warn "未找到 mysqladmin，跳过数据库连通预检"
    fi

    if is_port_in_use "${server_port}"; then
        log_err "端口 ${server_port} 已被占用，服务无法启动。"
        if command -v lsof >/dev/null 2>&1; then
            log_info "占用进程："
            lsof -iTCP:"${server_port}" -sTCP:LISTEN || true
        fi
        log_warn "可修改 .env 中 SERVER_PORT，或先停止占用该端口的进程。"
        return 1
    fi

    log_info "启动服务: ${BINARY_PATH}"
    log_info "停止服务请按 Ctrl+C"
    "${BINARY_PATH}"
}

run_health_check() {
    print_banner
    local checker
    checker="$(find_existing_path "${HEALTH_CHECK_CANDIDATES[@]}" || true)"
    if [[ -n "${checker}" && -x "${checker}" ]]; then
        log_info "执行项目健康检查..."
        "${checker}"
    else
        log_warn "未找到可执行的健康检查脚本。"
        log_warn "已尝试路径:"
        log_warn "  - ${HEALTH_CHECK_CANDIDATES[0]}"
        log_warn "  - ${HEALTH_CHECK_CANDIDATES[1]}"
        log_warn "  - ${HEALTH_CHECK_CANDIDATES[2]}"
        log_warn "可先执行: ./build.sh check"
    fi
}

docker_deploy() {
    print_banner
    if [[ -f "${PROJECT_DIR}/docker-compose.yml" ]]; then
        if command -v docker >/dev/null 2>&1; then
            log_info "使用 Docker Compose 部署..."
            docker compose up --build -d
            log_ok "Docker 部署完成"
        else
            log_err "未安装 docker，无法执行 Docker 部署"
            return 1
        fi
    else
        log_warn "当前项目没有 docker-compose.yml，已跳过 Docker 部署。"
    fi
}

full_deploy() {
    check_dependencies
    build_project
    create_config
    create_storage
    log_info "如果需要初始化数据库，请执行: ./build.sh init-db"
    local logic_test
    logic_test="$(find_existing_path "${LOGIC_TEST_CANDIDATES[@]}" || true)"
    if [[ -n "${logic_test}" ]]; then
        if [[ "${logic_test}" == *"tests/functional_test.sh" ]]; then
            log_info "如果需要运行功能测试，请执行: bash ${logic_test}"
        else
            log_info "如果需要运行功能测试，请执行: ${logic_test}"
        fi
    else
        log_warn "未找到逻辑测试脚本（已检查 scripts/tools/tests 常见位置）"
    fi
    log_ok "完整流程完成"
}

show_help() {
    cat <<'HELP'
用法:
  ./build.sh [command]

可用命令:
  check         检查依赖与环境
  install-deps  安装 Linux/WSL2 依赖
  build         配置并编译项目
  init-db       初始化数据库（执行 init.sql）
  config        生成 .env 配置
  storage       创建 storage 目录
  run           启动服务
  health        执行项目健康检查脚本
  full          执行完整流程（check/build/config/storage）
  docker        Docker Compose 部署（若存在 docker-compose.yml）
  menu          打开交互菜单
  help          显示帮助
HELP
}

show_menu() {
    echo
    echo "请选择操作:"
    echo "1. 检查依赖 (check)"
    echo "2. 安装依赖 (install-deps)"
    echo "3. 编译项目 (build)"
    echo "4. 初始化数据库 (init-db)"
    echo "5. 生成配置文件 (config)"
    echo "6. 创建存储目录 (storage)"
    echo "7. 启动服务 (run)"
    echo "8. 健康检查 (health)"
    echo "9. 完整流程 (full)"
    echo "10. Docker 部署 (docker)"
    echo "0. 退出"
    echo
    read -r -p "请输入选项 [0-10]: " choice

    case "$choice" in
        1) check_dependencies ;;
        2) install_libraries ;;
        3) build_project ;;
        4) init_database ;;
        5) create_config ;;
        6) create_storage ;;
        7) run_server ;;
        8) run_health_check ;;
        9) full_deploy ;;
        10) docker_deploy ;;
        0) log_info "退出"; exit 0 ;;
        *) log_err "无效选项" ;;
    esac

    show_menu
}

main() {
    local cmd="${1:-menu}"
    case "$cmd" in
        check) check_dependencies ;;
        install-deps) install_libraries ;;
        build) build_project ;;
        init-db) init_database ;;
        config) create_config ;;
        storage) create_storage ;;
        run) run_server ;;
        health) run_health_check ;;
        full) full_deploy ;;
        docker) docker_deploy ;;
        menu) show_menu ;;
        help|-h|--help) show_help ;;
        *)
            log_err "未知命令: $cmd"
            show_help
            return 1
            ;;
    esac
}

main "$@"
