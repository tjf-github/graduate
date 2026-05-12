#!/usr/bin/env bash
set -euo pipefail

APP_DIR="/opt/cloudisk"
SERVICE_NAME="cloudisk"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

if [[ "${EUID}" -eq 0 ]]; then
  echo "Please run this script as a normal user with sudo privileges, not as root."
  exit 1
fi

if ! command -v sudo >/dev/null 2>&1; then
  echo "sudo is required."
  exit 1
fi

echo "==> Installing dependencies"
sudo apt-get update
sudo apt-get install -y build-essential cmake mysql-server mysql-client libmysqlclient-dev libssl-dev uuid-dev

echo "==> Preparing app directory: ${APP_DIR}"
sudo mkdir -p "${APP_DIR}"
sudo chown -R "${USER}:${USER}" "${APP_DIR}"
rsync -a --delete ./ "${APP_DIR}/"

echo "==> Preparing MySQL"
sudo systemctl enable --now mysql

read -r -s -p "Enter MySQL password for app user 'cloudisk': " DB_PASSWORD
echo
if [[ -z "${DB_PASSWORD}" ]]; then
  echo "Password cannot be empty."
  exit 1
fi

sudo mysql -e "CREATE DATABASE IF NOT EXISTS cloudisk DEFAULT CHARACTER SET utf8mb4;"
sudo mysql -e "CREATE USER IF NOT EXISTS 'cloudisk'@'localhost' IDENTIFIED BY '${DB_PASSWORD}';"
sudo mysql -e "ALTER USER 'cloudisk'@'localhost' IDENTIFIED BY '${DB_PASSWORD}';"
sudo mysql -e "GRANT ALL PRIVILEGES ON cloudisk.* TO 'cloudisk'@'localhost'; FLUSH PRIVILEGES;"
mysql -u cloudisk -p"${DB_PASSWORD}" cloudisk < "${APP_DIR}/init.sql"

echo "==> Writing .env"
if [[ -f "${APP_DIR}/.env.example" ]]; then
  cp "${APP_DIR}/.env.example" "${APP_DIR}/.env"
elif [[ ! -f "${APP_DIR}/.env" ]]; then
  cat > "${APP_DIR}/.env" <<'EOF'
SERVER_PORT=8080
STORAGE_PATH=/opt/cloudisk/storage
STATIC_DIR=/opt/cloudisk/static
DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=cloudisk
DB_PASSWORD=change_me
DB_NAME=cloudisk
EOF
fi

sed -i "s|^SERVER_PORT=.*|SERVER_PORT=8080|" "${APP_DIR}/.env"
sed -i "s|^STORAGE_PATH=.*|STORAGE_PATH=/opt/cloudisk/storage|" "${APP_DIR}/.env"
sed -i "s|^STATIC_DIR=.*|STATIC_DIR=/opt/cloudisk/static|" "${APP_DIR}/.env"
sed -i "s|^DB_HOST=.*|DB_HOST=127.0.0.1|" "${APP_DIR}/.env"
sed -i "s|^DB_PORT=.*|DB_PORT=3306|" "${APP_DIR}/.env"
sed -i "s|^DB_USER=.*|DB_USER=cloudisk|" "${APP_DIR}/.env"
sed -i "s|^DB_PASSWORD=.*|DB_PASSWORD=${DB_PASSWORD}|" "${APP_DIR}/.env"
sed -i "s|^DB_NAME=.*|DB_NAME=cloudisk|" "${APP_DIR}/.env"

mkdir -p "${APP_DIR}/storage"

echo "==> Building project"
cd "${APP_DIR}"
cmake -S . -B build
cmake --build build -j"$(nproc)"

echo "==> Installing systemd service"
sudo tee "${SERVICE_FILE}" >/dev/null <<EOF
[Unit]
Description=Lightweight Comm Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
WorkingDirectory=${APP_DIR}
EnvironmentFile=${APP_DIR}/.env
ExecStart=${APP_DIR}/build/lightweight_comm_server
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now "${SERVICE_NAME}"

echo "==> Opening port 8080 (if UFW is enabled)"
sudo ufw allow 8080/tcp >/dev/null 2>&1 || true

echo "==> Service status"
sudo systemctl status "${SERVICE_NAME}" --no-pager || true

echo "==> Health check"
curl -I http://127.0.0.1:8080/ || true

echo
echo "Deployment finished."
