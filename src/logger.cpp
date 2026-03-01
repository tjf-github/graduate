#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

Logger::Logger() : currentLevel_(LogLevel::DEBUG)
{
    // Open log file in append mode.
    // If you want to overwrite every time, remove std::ios::app
    log_file_.open("logger.txt", std::ios::out | std::ios::app);
    if (!log_file_.is_open())
    {
        std::cerr << "Failed to open logger.txt" << std::endl;
    }
}

Logger::~Logger()
{
    if (log_file_.is_open())
    {
        log_file_.close();
    }
}

void Logger::setLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

void Logger::log(LogLevel level, const std::string &message)
{
    // If message level is lower than current level, ignore
    if (level < currentLevel_)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);

    // Output to console
    if (level == LogLevel::ERROR)
    {
        std::cerr << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
    }
    else
    {
        std::cout << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
    }

    if (log_file_.is_open())
    {
        log_file_ << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
        log_file_.flush();
    }
}

std::string Logger::getTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

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
