#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>

/**
 * @brief 日志级别枚举
 */
enum class LogLevel
{
    DEBUG, // 调试：开发调试时的详细信息
    INFO,  // 信息：正常的系统运行状态
    WARN,  // 警告：潜在的问题，但不影响运行
    ERROR  // 错误：导致功能失效的严重问题
};

/**
 * @brief 日志管理类（单例模式）
 * 负责将系统运行信息输出到控制台和日志文件
 */
class Logger
{
public:
    // 获取日志单例实例
    static Logger &getInstance();

    // 禁止拷贝构造和赋值操作，确保单例唯一性
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    // 输出日志到文件和控制台
    void log(LogLevel level, const std::string &message);

    // 设置当前日志过滤级别（低于此级别的日志将不被输出）
    void setLevel(LogLevel level);

private:
    Logger();  // 私有构造函数
    ~Logger(); // 私有析构函数

    // 获取格式化后的当前系统时间戳
    std::string getTimestamp();

    // 将枚举类型的日志级别转换为可读字符串
    std::string levelToString(LogLevel level);

    std::mutex mutex_;       // 互斥锁，保证多线程环境下写入日志的安全性
    LogLevel currentLevel_;  // 当前设定的日志过滤级别
    std::ofstream log_file_; // 日志文件输出流
};

// 为了方便调用，定义的日志快捷宏
#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::INFO, msg)
#define LOG_WARN(msg) Logger::getInstance().log(LogLevel::WARN, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)

#endif // LOGGER_H
