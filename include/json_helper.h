#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <string>
#include <map>
#include <vector>

// 简单的JSON构建器类--序列化
class JsonBuilder {
public:
    JsonBuilder& add(const std::string& key, const std::string& value);
    JsonBuilder& add(const std::string& key, int value);
    JsonBuilder& add(const std::string& key, long long value);
    JsonBuilder& add(const std::string& key, bool value);
    JsonBuilder& add_null(const std::string& key);
    
    std::string build();
    
private:
    // 存储键值对的容器
    std::map<std::string, std::string> data;
    // 转义字符串中的特殊字符
    std::string escape_json(const std::string& str);
};

// 简单的JSON解析器--反序列化
class JsonParser {
public:
    explicit JsonParser(const std::string& json_str);
    
    bool has(const std::string& key) const;
    std::string get_string(const std::string& key, 
                          const std::string& default_value = "") const;
    int get_int(const std::string& key, int default_value = 0) const;
    long long get_long(const std::string& key, 
                       long long default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;
    
private:
    // 存储解析后的键值对
    std::map<std::string, std::string> data;
    // 解析JSON字符串
    void parse(const std::string& json_str);
    // 去除字符串两端的空白字符
    std::string trim(const std::string& str);
};

#endif // JSON_HELPER_H
