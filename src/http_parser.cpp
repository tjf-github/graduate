#include "http_parser.h"

#include <sstream>
#include "utils.h"

// ---------------------------------------------------------
// 2. HttpParser 的实现（解决 Undefined Reference 的关键）
// ---------------------------------------------------------

// 注意：这里不需要写 static 关键字
HttpRequest HttpParser::parse(const std::string &raw_request)
{
    HttpRequest req;
    std::istringstream iss(raw_request);

    // 1. 解析请求行: GET /index.html HTTP/1.1
    std::string line;
    if (!std::getline(iss, line) || line.empty())
        return req;

    // 去除行尾的 \r
    if (line.back() == '\r')
        line.pop_back();

    std::istringstream line_iss(line);
    line_iss >> req.method >> req.path >> req.version;

    size_t query_pos = req.path.find('?');
    if (query_pos != std::string::npos)
    {
        std::string query_str = req.path.substr(query_pos + 1);
        req.path = req.path.substr(0, query_pos);

        size_t start = 0;
        while (start < query_str.length())
        {
            size_t end = query_str.find('&', start);
            if (end == std::string::npos)
                end = query_str.length();

            std::string pair = query_str.substr(start, end - start);
            size_t eq = pair.find('=');
            if (eq != std::string::npos)
            {
                std::string key = pair.substr(0, eq);
                std::string value = url_decode(pair.substr(eq + 1));
                req.params[key] = value;
            }
            else if (!pair.empty())
            {
                req.params[pair] = "";
            }

            start = end + 1;
        }
    }

    // 2. 解析 Headers
    int content_length = 0;
    while (std::getline(iss, line) && !line.empty() && line != "\r")
    {
        if (line.back() == '\r')
            line.pop_back();
        if (line.empty())
            break; // 空行，Headers 结束

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos)
        {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim whitespace
            while (!value.empty() && value.front() == ' ')
                value.erase(0, 1);

            req.headers[key] = value;

            if (key == "Content-Length")
            {
                try
                {
                    content_length = std::stoi(value);
                }
                catch (...)
                {
                    content_length = 0;
                }
            }
        }
    }

    // 3. 解析 Body (如果 Content-Length > 0)
    // 此时 iss 指针正位于空行之后，也就是 Body 的起始位置
    if (content_length > 0)
    {
        // 计算当前读到的位置在原始字符串中的索引
        // 注意：tellg 在某些流实现中可能不准，更稳健的方法是把剩下的流全部读出来
        // 或者简单地：找到 \r\n\r\n 分隔符，取后面的部分

        size_t header_end = raw_request.find("\r\n\r\n");
        if (header_end != std::string::npos)
        {
            size_t body_start = header_end + 4;
            if (body_start < raw_request.length())
            {
                req.body = raw_request.substr(body_start, content_length);
            }
        }
    }

    return req;
}

// 构建HTTP响应字符串
std::string HttpParser::build_response(const HttpResponse &response)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";

    // 使用 response 中设置的 headers, 如果没有 Content-Type 则默认 application/json (虽然构造函数已设置)
    for (const auto &header : response.headers)
    {
        oss << header.first << ": " << header.second << "\r\n";
    }

    // 可以在这里计算 Content-Length，如果 headers 里没有的话
    if (response.headers.find("Content-Length") == response.headers.end())
    {
        oss << "Content-Length: " << response.body.length() << "\r\n";
    }

    // 强制关闭连接以简化处理
    if (response.headers.find("Connection") == response.headers.end())
    {
        oss << "Connection: close\r\n";
    }

    oss << "\r\n";
    oss << response.body;
    return oss.str();
}
