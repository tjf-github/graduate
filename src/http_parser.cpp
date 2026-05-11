#include "http_parser.h"

#include <sstream>
#include <algorithm>
#include "utils.h"

// 解析原始 HTTP 报文为结构化请求对象
HttpRequest HttpParser::parse(const std::string &raw_request)
{
    HttpRequest req;
    std::istringstream iss(raw_request);

    // 1) 解析请求行: GET /index.html HTTP/1.1
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

    // 2) 解析 Headers
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

            // 规范化 header 名为小写，符合 HTTP 大小写不敏感规范
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            // 去掉 header value 前导空格
            while (!value.empty() && value.front() == ' ')
                value.erase(0, 1);

            req.headers[key] = value;

            if (key == "content-length")
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

    // 3) 若存在 Content-Length，按长度截取 body
    if (content_length > 0)
    {
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

// 将响应对象编码为 HTTP 文本
std::string HttpParser::build_response(const HttpResponse &response)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";

    // 输出调用方已设置的 headers
    for (const auto &header : response.headers)
    {
        oss << header.first << ": " << header.second << "\r\n";
    }

    // 缺省补齐 Content-Length
    if (response.headers.find("Content-Length") == response.headers.end())
    {
        oss << "Content-Length: " << response.body.length() << "\r\n";
    }

    // 默认短连接，简化服务端实现
    if (response.headers.find("Connection") == response.headers.end())
    {
        oss << "Connection: close\r\n";
    }

    oss << "\r\n";
    oss << response.body;
    return oss.str();
}
