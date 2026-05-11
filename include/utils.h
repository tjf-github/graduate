#ifndef UTILS_H
#define UTILS_H

#include <string>

std::string current_time_iso8601();
// 本地时区时间戳（用于日志展示）
std::string getTimestamp();
// URL 解码：处理 `%xx` 与 `+`
std::string url_decode(const std::string &str);
// URL 编码（UTF-8 字节级别百分号转义）
std::string url_encode_utf8(const std::string &str);

#endif
