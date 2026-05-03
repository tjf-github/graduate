#include "utils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cctype>

std::string current_time_iso8601()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm_value;
#ifdef _WIN32
    gmtime_s(&tm_value, &now_time);
#else
    gmtime_r(&now_time, &tm_value);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string getTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string url_decode(const std::string &str)
{
    std::string res;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.length())
        {
            int value;
            std::stringstream ss;
            ss << std::hex << str.substr(i + 1, 2);
            ss >> value;
            res += static_cast<char>(value);
            i += 2;
        }
        else if (str[i] == '+')
        {
            res += ' ';
        }
        else
        {
            res += str[i];
        }
    }
    return res;
}

std::string url_encode_utf8(const std::string &str)
{
    std::ostringstream encoded;
    for (unsigned char c : str)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            encoded << c;
        else
            encoded << '%' << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << (int)c;
    }
    return encoded.str();
}
