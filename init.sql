CREATE DATABASE IF NOT EXISTS cloudisk;
USE cloudisk;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(100) UNIQUE,
    storage_used BIGINT NOT NULL DEFAULT 0,
    storage_limit BIGINT NOT NULL DEFAULT 10737418240,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 文件表
CREATE TABLE IF NOT EXISTS files (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    filename VARCHAR(255) NOT NULL,
    original_filename VARCHAR(255) NOT NULL,
    file_path VARCHAR(512) NOT NULL,
    file_size BIGINT NOT NULL,
    mime_type VARCHAR(100),
    share_code VARCHAR(64) UNIQUE,
    upload_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 分片上传会话表
CREATE TABLE IF NOT EXISTS upload_sessions (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    upload_id VARCHAR(64) NOT NULL UNIQUE,
    total_chunks INT NOT NULL,
    file_size BIGINT NOT NULL,
    original_filename VARCHAR(255) NOT NULL,
    mime_type VARCHAR(100),
    status VARCHAR(32) NOT NULL DEFAULT 'uploading',
    expires_at TIMESTAMP NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 分片上传块表
CREATE TABLE IF NOT EXISTS upload_chunks (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    upload_id VARCHAR(64) NOT NULL,
    chunk_index INT NOT NULL,
    chunk_hash VARCHAR(64) NOT NULL,
    chunk_size BIGINT NOT NULL,
    status VARCHAR(32) NOT NULL DEFAULT 'completed',
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_upload_chunk (upload_id, chunk_index),
    FOREIGN KEY (upload_id) REFERENCES upload_sessions(upload_id) ON DELETE CASCADE
);

-- 会话表（当前代码主要使用内存会话，这里保留库表）
CREATE TABLE IF NOT EXISTS sessions (
    id VARCHAR(100) PRIMARY KEY,
    user_id INT NOT NULL,
    token VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 消息表
CREATE TABLE IF NOT EXISTS messages (
    id INT AUTO_INCREMENT PRIMARY KEY,
    sender_id INT NOT NULL,
    receiver_id INT NOT NULL,
    content TEXT NOT NULL,
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (receiver_id) REFERENCES users(id) ON DELETE CASCADE
);

-- 可选分享码表（当前代码通过 files.share_code 工作，保留此表用于后续扩展）
CREATE TABLE IF NOT EXISTS share_codes (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    file_id INT NOT NULL,
    code VARCHAR(64) NOT NULL UNIQUE,
    created_by INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NULL,
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE CASCADE
);

-- 兼容旧版本表结构：按需补齐字段
SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'users' AND column_name = 'storage_used'
);
SET @sql = IF(@exists = 0, 'ALTER TABLE users ADD COLUMN storage_used BIGINT NOT NULL DEFAULT 0', 'SELECT "users.storage_used exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'users' AND column_name = 'storage_limit'
);
SET @sql = IF(@exists = 0, 'ALTER TABLE users ADD COLUMN storage_limit BIGINT NOT NULL DEFAULT 10737418240', 'SELECT "users.storage_limit exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'files' AND column_name = 'original_filename'
);
SET @sql = IF(@exists = 0, 'ALTER TABLE files ADD COLUMN original_filename VARCHAR(255) NOT NULL DEFAULT "uploaded_file"', 'SELECT "files.original_filename exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'files' AND column_name = 'mime_type'
);
SET @sql = IF(@exists = 0, 'ALTER TABLE files ADD COLUMN mime_type VARCHAR(100)', 'SELECT "files.mime_type exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'files' AND column_name = 'upload_date'
);
SET @sql = IF(@exists = 0, 'ALTER TABLE files ADD COLUMN upload_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP', 'SELECT "files.upload_date exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'files' AND column_name = 'share_code'
);
SET @sql = IF(@exists = 0, 'ALTER TABLE files ADD COLUMN share_code VARCHAR(64) UNIQUE', 'SELECT "files.share_code exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 若旧字段存在，做一次数据迁移
SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'files' AND column_name = 'file_type'
);
SET @sql = IF(@exists > 0, 'UPDATE files SET mime_type = COALESCE(mime_type, file_type)', 'SELECT "files.file_type missing"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'files' AND column_name = 'upload_time'
);
SET @sql = IF(@exists > 0, 'UPDATE files SET upload_date = COALESCE(upload_date, upload_time)', 'SELECT "files.upload_time missing"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 幂等创建索引：避免重复执行 init.sql 时出现 Duplicate key name
SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'files' AND index_name = 'idx_user_id'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_user_id ON files(user_id)', 'SELECT "idx_user_id exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'files' AND index_name = 'idx_files_user_upload_date'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_files_user_upload_date ON files(user_id, upload_date)', 'SELECT "idx_files_user_upload_date exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'files' AND index_name = 'idx_files_share_code'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_files_share_code ON files(share_code)', 'SELECT "idx_files_share_code exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'upload_sessions' AND index_name = 'idx_upload_sessions_user_id'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_upload_sessions_user_id ON upload_sessions(user_id)', 'SELECT "idx_upload_sessions_user_id exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'upload_sessions' AND index_name = 'idx_upload_sessions_expires_at'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_upload_sessions_expires_at ON upload_sessions(expires_at)', 'SELECT "idx_upload_sessions_expires_at exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'upload_chunks' AND index_name = 'idx_upload_chunks_upload_id_status'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_upload_chunks_upload_id_status ON upload_chunks(upload_id, status)', 'SELECT "idx_upload_chunks_upload_id_status exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'sessions' AND index_name = 'idx_session_user'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_session_user ON sessions(user_id)', 'SELECT "idx_session_user exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'messages' AND index_name = 'idx_messages_receiver_sender_read'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_messages_receiver_sender_read ON messages(receiver_id, sender_id, is_read)', 'SELECT "idx_messages_receiver_sender_read exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @exists = (
    SELECT COUNT(*) FROM information_schema.statistics
    WHERE table_schema = DATABASE() AND table_name = 'messages' AND index_name = 'idx_messages_created_at'
);
SET @sql = IF(@exists = 0, 'CREATE INDEX idx_messages_created_at ON messages(created_at)', 'SELECT "idx_messages_created_at exists"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
