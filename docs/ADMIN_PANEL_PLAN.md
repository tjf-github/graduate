# 后台管理系统设计方案

## 1. 目标

为当前网盘系统补齐后台管理能力，重点解决“看不到用户做了什么”的问题，支持管理员登录、用户管理、活动追踪、基础统计与审计。

## 2. MVP 范围（建议先做）

1. 管理员登录与权限校验
2. 用户列表与用户详情
3. 用户活动日志记录与查询
4. 仪表板基础统计（今日上传/下载、活跃用户）

## 3. 活动日志设计

建议新增 `user_activity_log` 表：

```sql
CREATE TABLE user_activity_log (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    action_type VARCHAR(50) NOT NULL,
    action_details JSON,
    ip_address VARCHAR(50),
    user_agent VARCHAR(255),
    status ENUM('success', 'failed') DEFAULT 'success',
    error_message VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    INDEX idx_user_time (user_id, created_at DESC),
    INDEX idx_action_time (action_type, created_at DESC),
    INDEX idx_created_at (created_at DESC)
);
```

`action_type` 建议统一枚举值：
- `login`
- `logout`
- `upload`
- `download`
- `delete`
- `rename`
- `mkdir`
- `share`

## 4. 管理员与审计

建议新增 `admin_users` 与 `admin_audit_log`：

```sql
CREATE TABLE admin_users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin','operator','viewer') DEFAULT 'operator',
    is_active BOOLEAN DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL
);
```

```sql
CREATE TABLE admin_audit_log (
    id INT PRIMARY KEY AUTO_INCREMENT,
    admin_id INT NOT NULL,
    action VARCHAR(100) NOT NULL,
    target_type VARCHAR(50),
    target_id INT,
    changes JSON,
    ip_address VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_admin_time (admin_id, created_at DESC)
);
```

## 5. API 设计（MVP）

认证：
- `POST /api/admin/login`
- `POST /api/admin/logout`

用户管理：
- `GET /api/admin/users?page=1&limit=20&search=`
- `GET /api/admin/users/{id}`
- `PUT /api/admin/users/{id}/status`

活动日志：
- `GET /api/admin/activity/logs?page=1&limit=50&action_type=&start_time=&end_time=`
- `GET /api/admin/users/{id}/activity?page=1&limit=30`

仪表板：
- `GET /api/admin/dashboard/overview`

## 6. 安全要求

1. 管理员接口必须校验独立 token
2. 高风险操作需记录审计日志
3. 日志中禁止保存密码、token 明文
4. 日志查询接口增加分页与时间范围限制
5. 建议设置日志保留策略（例如 90 天）

## 7. 实施顺序（2-3 天）

第 1 天：
1. 建表（`user_activity_log`、`admin_users`、`admin_audit_log`）
2. 完成管理员登录 + token 校验
3. 在上传/下载/删除链路埋点

第 2 天：
1. 完成用户列表和活动日志查询 API
2. 完成 `dashboard` 基础统计 API
3. 前端做最小可用页面联调

第 3 天（可选）：
1. 增加图表统计与筛选器
2. 增加管理员操作审计页面
3. 做权限分级（admin/operator/viewer）
