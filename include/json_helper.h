#ifndef JSON_HELPER_H
#define JSON_HELPER_H

#include <string>
#include <map>
#include <vector>

// 简单的JSON构建器类
class JsonBuilder {
public:
    JsonBuilder& add(const std::string& key, const std::string& value);
    JsonBuilder& add(const std::string& key, int value);
    JsonBuilder& add(const std::string& key, long long value);
    JsonBuilder& add(const std::string& key, bool value);
    JsonBuilder& add_null(const std::string& key);
    
    std::string build();
    
private:
    std::map<std::string, std::string> data;
    
    std::string escape_json(const std::string& str);
};

// 简单的JSON解析器
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
    std::map<std::string, std::string> data;
    
    void parse(const std::string& json_str);
    std::string trim(const std::string& str);
};

#endif // JSON_HELPER_H
