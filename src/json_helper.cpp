#include "json_helper.h"
#include <sstream>
#include <algorithm>

// JsonBuilder实现
//处理特殊字符转义
std::string JsonBuilder::escape_json(const std::string& str) {
    std::string result;
    for(char c : str) {
        switch(c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

JsonBuilder& JsonBuilder::add(const std::string& key, const std::string& value) {
    data[key] = "\"" + escape_json(value) + "\"";
    return *this;
}

JsonBuilder& JsonBuilder::add(const std::string& key, int value) {
    data[key] = std::to_string(value);
    return *this;
}

JsonBuilder& JsonBuilder::add(const std::string& key, long long value) {
    data[key] = std::to_string(value);
    return *this;
}

JsonBuilder& JsonBuilder::add(const std::string& key, bool value) {
    data[key] = value ? "true" : "false";
    return *this;
}

JsonBuilder& JsonBuilder::add_null(const std::string& key) {
    data[key] = "null";
    return *this;
}

std::string JsonBuilder::build() {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for(const auto& pair : data) {
        if(!first) oss << ",";
        oss << "\"" << escape_json(pair.first) << "\":" << pair.second;
        first = false;
    }
    
    oss << "}";
    return oss.str();
}

// JsonParser实现
JsonParser::JsonParser(const std::string& json_str) {
    parse(json_str);
}

std::string JsonParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if(start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

void JsonParser::parse(const std::string& json_str) {
    // 简单的JSON解析（仅支持平面对象）
    std::string content = trim(json_str);
    
    if(content.empty() || content.front() != '{' || content.back() != '}') {
        return;
    }
    
    content = content.substr(1, content.length() - 2);
    
    size_t pos = 0;
    while(pos < content.length()) {
        // 查找键
        size_t key_start = content.find('"', pos);
        if(key_start == std::string::npos) break;
        
        size_t key_end = content.find('"', key_start + 1);
        if(key_end == std::string::npos) break;
        
        std::string key = content.substr(key_start + 1, key_end - key_start - 1);
        
        // 查找冒号
        size_t colon = content.find(':', key_end);
        if(colon == std::string::npos) break;
        
        // 查找值
        pos = colon + 1;
        while(pos < content.length() && std::isspace(content[pos])) pos++;
        
        std::string value;
        if(content[pos] == '"') {
            // 字符串值
            size_t value_end = content.find('"', pos + 1);
            if(value_end == std::string::npos) break;
            value = content.substr(pos + 1, value_end - pos - 1);
            pos = value_end + 1;
        } else {
            // 数字、布尔或null
            size_t value_end = content.find_first_of(",}", pos);
            if(value_end == std::string::npos) value_end = content.length();
            value = trim(content.substr(pos, value_end - pos));
            pos = value_end;
        }
        
        data[key] = value;
        
        // 跳过逗号
        if(pos < content.length() && content[pos] == ',') pos++;
    }
}

bool JsonParser::has(const std::string& key) const {
    return data.find(key) != data.end();
}

std::string JsonParser::get_string(const std::string& key, 
                                   const std::string& default_value) const {
    auto it = data.find(key);
    return (it != data.end()) ? it->second : default_value;
}

int JsonParser::get_int(const std::string& key, int default_value) const {
    auto it = data.find(key);
    if(it == data.end()) return default_value;
    
    try {
        return std::stoi(it->second);
    } catch(...) {
        return default_value;
    }
}

long long JsonParser::get_long(const std::string& key, 
                               long long default_value) const {
    auto it = data.find(key);
    if(it == data.end()) return default_value;
    
    try {
        return std::stoll(it->second);
    } catch(...) {
        return default_value;
    }
}

bool JsonParser::get_bool(const std::string& key, bool default_value) const {
    auto it = data.find(key);
    if(it == data.end()) return default_value;
    
    return it->second == "true";
}
