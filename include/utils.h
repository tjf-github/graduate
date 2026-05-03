#ifndef UTILS_H
#define UTILS_H

#include <string>

std::string current_time_iso8601();
std::string getTimestamp();
std::string url_decode(const std::string &str);
std::string url_encode_utf8(const std::string &str);

#endif
