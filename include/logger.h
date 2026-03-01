#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>

enum class LogLevel
{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger
{
public:
    static Logger &getInstance();

    // Delete copy constructor and assignment operator
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void log(LogLevel level, const std::string &message);
    void setLevel(LogLevel level);

private:
    Logger();
    ~Logger();

    std::string getTimestamp();
    std::string levelToString(LogLevel level);

    std::mutex mutex_;
    LogLevel currentLevel_;
    std::ofstream log_file_;
};

#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::INFO, msg)
#define LOG_WARN(msg) Logger::getInstance().log(LogLevel::WARN, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)

#endif // LOGGER_H
