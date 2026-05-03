#include "logger.h"
#include "utils.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

// 获取日志单例实例，全局唯一
Logger &Logger::getInstance()
{
    // C++11 起，局部静态变量的初始化是线程安全的
    static Logger instance;
    return instance;
}

// 构造函数：初始化日志级别并打开日志文件
Logger::Logger() : currentLevel_(LogLevel::DEBUG)
{
    // 以追加模式(std::ios::app)打开日志文件
    // 如果希望每次运行都覆盖旧日志，可以去掉 std::ios::app
    log_file_.open("logger.txt", std::ios::out | std::ios::app);
    if (!log_file_.is_open())
    {
        std::cerr << "无法打开日志文件 logger.txt" << std::endl;
    }
}

// 析构函数：确保程序退出时关闭文件流
Logger::~Logger()
{
    if (log_file_.is_open())
    {
        log_file_.close();
    }
}

// 设置日志过滤级别，使用互斥锁保证线程安全
void Logger::setLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

// 核心日志记录方法：输出到控制台并保存到文件
void Logger::log(LogLevel level, const std::string &message)
{
    // 如果日志级别低于当前设置的过滤级别，则直接丢弃
    if (level < currentLevel_)
    {
        return;
    }

    // 加锁，防止多个线程同时写入导致内容混乱
    std::lock_guard<std::mutex> lock(mutex_);

    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);

    // 根据级别输出到标准输出(stdout)或标准错误(stderr)
    if (level == LogLevel::ERROR)
    {
        std::cerr << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
    }
    else
    {
        std::cout << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
    }

    // 如果文件流正常，将日志写入文件并立即刷新到磁盘
    if (log_file_.is_open())
    {
        log_file_ << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
        log_file_.flush(); // 强制刷新缓冲区，确保日志不因程序意外终止而丢失
    }
}

// 获取当前系统时间并格式化为 "YYYY-MM-DD HH:MM:SS"
std::string Logger::getTimestamp()
{
    return ::getTimestamp();
}

// 将日志级别枚举映射为对应的字符串，方便显示
std::string Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
