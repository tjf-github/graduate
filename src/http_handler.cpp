#include "http_handler.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include "json_helper.h"
#include "logger.h"

std::string url_decode(const std::string &str)
{
    std::string res;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.length())
        {
            // 将 16 进制转为字符
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

// ---------------------------------------------------------
// 1. HttpHandler 的实现
// ---------------------------------------------------------

HttpHandler::HttpHandler(std::shared_ptr<UserManager> user_mgr,
                         std::shared_ptr<FileManager> file_mgr,
                         std::shared_ptr<SessionManager> session_mgr)
    : user_manager(user_mgr), file_manager(file_mgr), session_manager(session_mgr) {}

// 根据请求路径和方法调用相应的处理函数，返回HTTP响应
HttpResponse HttpHandler::handle_request(const HttpRequest &request)
{
    if (request.path == "/api/register" && request.method == "POST")
        return handle_register(request);
    if (request.path == "/api/login" && request.method == "POST")
        return handle_login(request);
    if (request.path == "/api/user/info" && request.method == "GET")
        return handle_user_info(request);

    // 文件操作
    if (request.path == "/api/file/list" && request.method == "GET")
        return handle_file_list(request);
    if (request.path == "/api/file/upload" && request.method == "POST")
        return handle_file_upload(request);
    if (request.path.find("/api/file/download") == 0 && request.method == "GET")
        return handle_file_download(request);
    if (request.path == "/api/file/delete" && request.method == "POST")
        return handle_file_delete(request);
    if (request.path == "/api/file/rename" && request.method == "POST")
        return handle_file_rename(request);
    if (request.path == "/api/file/search" && request.method == "GET")
        return handle_file_search(request);

    // 静态文件服务：简单的根目录映射
    std::string file_path;
    if (request.path == "/" || request.path == "/index.html")
    {
        file_path = "static/index.html"; // 尝试相对路径
    }
    else
    {
        // 简单的安全检查，防止目录遍历攻击
        if (request.path.find("..") != std::string::npos)
        {
            return error_response(403, "Forbidden");
        }
        // 尝试直接映射 static 目录下的文件
        if (request.path.substr(0, 8) == "/static/")
        {
            file_path = request.path.substr(1); // 去掉开头的 /
        }
    }

    // 尝试打开文件，如果存在则返回内容，否则返回404
    if (!file_path.empty())
    {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open())
        {
            // 尝试上级目录（如果是在 build 目录运行）
            file.open("../" + file_path, std::ios::binary);
        }

        if (file.is_open())
        {
            std::ostringstream ss;
            ss << file.rdbuf();
            HttpResponse response;
            response.status_code = 200;
            response.status_text = "OK";
            response.body = ss.str();

            // 简单的 Content-Type 推断
            if (file_path.find(".html") != std::string::npos)
                response.headers["Content-Type"] = "text/html";
            else if (file_path.find(".css") != std::string::npos)
                response.headers["Content-Type"] = "text/css";
            else if (file_path.find(".js") != std::string::npos)
                response.headers["Content-Type"] = "application/javascript";
            else
                response.headers["Content-Type"] = "text/plain";

            return response;
        }
    }

    return error_response(404, "Not Found");
}

// 构建JSON格式的响应体，包含状态码、消息和可选的数据字段
HttpResponse HttpHandler::json_response(int code, const std::string &message, const std::string &data)
{
    HttpResponse response;
    std::ostringstream ss;
    ss << "{\"code\":" << code << ",\"message\":\"" << message << "\"";
    if (!data.empty())
        ss << ",\"data\":" << data;
    ss << "}";
    response.body = ss.str();
    response.status_code = code;
    return response;
}

// 构建错误响应，默认使用json_response来构建响应体
HttpResponse HttpHandler::error_response(int code, const std::string &message)
{
    return json_response(code, message);
}

// 存根实现，后续会完善这些函数的逻辑
HttpResponse HttpHandler::handle_register(const HttpRequest &request)
{
    LOG_DEBUG("Received Register Request. Body: " + request.body);

    JsonParser parser(request.body);
    std::string username = parser.get_string("username");
    std::string email = parser.get_string("email");
    std::string password = parser.get_string("password");

    LOG_DEBUG("Parsed: user=" + username + ", email=" + email + ", pass=" + password);

    if (username.empty() || email.empty() || password.empty())
    {
        LOG_WARN("Missing fields!");
        return error_response(400, "Missing required fields");
    }

    if (user_manager->register_user(username, email, password))
    {
        return json_response(200, "Register success");
    }
    else
    {
        LOG_WARN("Registration failed in UserManager");
        return error_response(400, "Username or email already exists");
    }
}

HttpResponse HttpHandler::handle_login(const HttpRequest &request)
{
    JsonParser parser(request.body);
    std::string username = parser.get_string("username");
    std::string password = parser.get_string("password");

    if (username.empty() || password.empty())
    {
        return error_response(400, "Missing username or password");
    }

    auto user = user_manager->login(username, password);
    if (user)
    {
        std::string token = session_manager->create_session(user->id);
        JsonBuilder builder;
        builder.add("token", token);
        builder.add("username", user->username);
        builder.add("id", user->id);
        return json_response(200, "Login success", builder.build());
    }
    else
    {
        return error_response(401, "Invalid username or password");
    }
}

HttpResponse HttpHandler::handle_logout(const HttpRequest &request)
{
    std::string token = get_session_token(request);
    if (token.empty())
    {
        return error_response(401, "Unauthorized");
    }

    session_manager->delete_session(token);
    return json_response(200, "Logout success");
}

HttpResponse HttpHandler::handle_user_info(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    // TODO: Ideally we should get user info from DB, but for now just mock or minimal
    /*
    auto user = user_manager->get_user(user_id);
    if(user) { ... }
    */
    JsonBuilder builder;
    builder.add("user_id", user_id);
    builder.add("active_sessions", static_cast<int>(session_manager->session_count()));
    builder.add("timeout_minutes", static_cast<int>(session_manager->get_timeout_minutes()));

    return json_response(200, "User info", builder.build());
}

HttpResponse HttpHandler::handle_file_list(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    int offset = 0;
    int limit = 10;
    if (request.params.count("offset"))
        offset = std::stoi(request.params.at("offset"));
    if (request.params.count("limit"))
        limit = std::stoi(request.params.at("limit"));

    std::vector<FileInfo> files = file_manager->list_user_files(user_id, offset, limit);

    // 手动构建JSON数组
    std::string json_array = "[";
    for (size_t i = 0; i < files.size(); ++i)
    {
        const auto &file = files[i];
        if (i > 0)
            json_array += ",";
        JsonBuilder builder;
        builder.add("id", file.id);
        builder.add("filename", file.filename);
        builder.add("original_filename", file.original_filename);
        builder.add("size", file.file_size);
        builder.add("upload_date", file.upload_date);
        json_array += builder.build();
    }
    json_array += "]";

    return json_response(200, "Success", json_array);
}

HttpResponse HttpHandler::handle_file_upload(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    std::string filename = "uploaded_file";

    // 逻辑合并：如果 Header 存在，只执行解码赋值
    if (request.headers.count("X-Filename"))
    {
        // 关键：只保留这一行，确保存入数据库的是解码后的中文
        filename = url_decode(request.headers.at("X-Filename"));
    }

    // 假设 body 即文件内容
    auto result = file_manager->upload_file(user_id, filename, "application/octet-stream",
                                            request.body.c_str(), request.body.size());

    if (result)
    {
        JsonBuilder builder;
        builder.add("file_id", result->id);
        // 这里返回给前端的也应该是解码后的 original_filename
        builder.add("filename", result->original_filename);
        return json_response(200, "Upload success", builder.build());
    }
    else
    {
        return error_response(500, "Upload failed");
    }
}
HttpResponse HttpHandler::handle_file_download(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    int file_id = -1;
    // 从参数获取ID
    if (request.params.count("id"))
    {
        file_id = std::stoi(request.params.at("id"));
    }
    else
    {
        // 尝试从路径获取 /api/file/download/123 (如果路由支持的话)
        size_t pos = request.path.find_last_of('/');
        if (pos != std::string::npos && pos < request.path.length() - 1)
        {
            try
            {
                file_id = std::stoi(request.path.substr(pos + 1));
            }
            catch (...)
            {
            }
        }
    }

    if (file_id == -1)
    {
        return error_response(400, "Missing file ID");
    }

    auto file_info = file_manager->get_file_info(file_id, user_id);
    if (!file_info)
    {
        return error_response(404, "File not found");
    }

    auto data = file_manager->download_file(file_id, user_id);
    if (data)
    {
        HttpResponse response;
        response.status_code = 200;
        response.status_text = "OK";
        response.headers["Content-Type"] = "application/octet-stream";
        response.headers["Content-Disposition"] = "attachment; filename=\"" + file_info->original_filename + "\"";
        response.body = std::string(data->begin(), data->end());
        return response;
    }
    else
    {
        return error_response(500, "Download failed");
    }
}

HttpResponse HttpHandler::handle_file_delete(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
        return error_response(401, "Unauthorized");

    JsonParser parser(request.body);
    int file_id = parser.get_int("id", -1);
    if (file_id == -1)
        return error_response(400, "Missing file ID");

    if (file_manager->delete_file(file_id, user_id))
    {
        return json_response(200, "File deleted");
    }
    else
    {
        return error_response(500, "Delete failed or file not found");
    }
}

HttpResponse HttpHandler::handle_file_rename(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
        return error_response(401, "Unauthorized");

    JsonParser parser(request.body);
    int file_id = parser.get_int("id", -1);
    std::string new_name = parser.get_string("new_name");

    if (file_id == -1 || new_name.empty())
        return error_response(400, "Missing parameters");

    if (file_manager->rename_file(file_id, user_id, new_name))
    {
        return json_response(200, "File renamed");
    }
    else
    {
        return error_response(500, "Rename failed");
    }
}

HttpResponse HttpHandler::handle_file_search(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
        return error_response(401, "Unauthorized");

    std::string keyword;
    if (request.params.count("keyword"))
        keyword = request.params.at("keyword");

    auto files = file_manager->search_files(user_id, keyword);

    std::string json_array = "[";
    for (size_t i = 0; i < files.size(); ++i)
    {
        if (i > 0)
            json_array += ",";
        JsonBuilder builder;
        builder.add("id", files[i].id);
        builder.add("filename", files[i].original_filename);
        json_array += builder.build();
    }
    json_array += "]";

    return json_response(200, "Success", json_array);
}

std::string HttpHandler::get_session_token(const HttpRequest &request)
{
    if (request.headers.count("Authorization"))
    {
        std::string auth = request.headers.at("Authorization");
        if (auth.substr(0, 7) == "Bearer ")
        {
            return auth.substr(7);
        }
    }
    return "";
}

int HttpHandler::get_user_id_from_session(const HttpRequest &request)
{
    std::string token = get_session_token(request);
    if (token.empty())
    {
        return -1;
    }
    return session_manager->get_user_id(token);
}

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
