-- =====================================================================
--  GameServer 数据库初始化脚本
--  创建用户 yy、数据库 gameserver、account 表
-- =====================================================================

CREATE DATABASE IF NOT EXISTS gameserver CHARACTER SET utf8mb4;

CREATE USER IF NOT EXISTS 'yy'@'%' IDENTIFIED BY '0';
CREATE USER IF NOT EXISTS 'yy'@'localhost' IDENTIFIED BY '0';
GRANT ALL PRIVILEGES ON *.* TO 'yy'@'%' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON *.* TO 'yy'@'localhost' WITH GRANT OPTION;

USE gameserver;

CREATE TABLE IF NOT EXISTS account (
    uid      BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(64)     NOT NULL,
    password VARCHAR(255)    NOT NULL,
    PRIMARY KEY (uid),
    UNIQUE KEY uk_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

FLUSH PRIVILEGES;
