#include "http_handler.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <vector>
#include "json_helper.h"
#include "logger.h"
#include "utils.h"


static int parse_int_param(const std::map<std::string, std::string> &params,
                           const std::string &key,
                           int default_value)
{
    auto it = params.find(key);
    if (it == params.end() || it->second.empty())
        return default_value;

    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return default_value;
    }
}

static std::string build_static_path(const std::string &request_path)
{
    const std::string static_root = std::getenv("STATIC_DIR") ? std::getenv("STATIC_DIR") : "static";
    const bool is_index = request_path == "/" || request_path == "/index.html";
    const std::string relative_path = is_index ? "index.html" : request_path.substr(8);

    std::vector<std::string> candidates = {
        static_root + "/" + relative_path,
        "../" + static_root + "/" + relative_path,
        "./" + static_root + "/" + relative_path};

    for (const auto &candidate : candidates)
    {
        std::ifstream file(candidate, std::ios::binary);
        if (file.is_open())
        {
            return candidate;
        }
    }

    return "";
}

// ---------------------------------------------------------
// 1. HttpHandler 的实现
// ---------------------------------------------------------

HttpHandler::HttpHandler(std::shared_ptr<UserManager> user_mgr,
                         std::shared_ptr<FileManager> file_mgr,
                         std::shared_ptr<SessionManager> session_mgr,
                         std::shared_ptr<ClientManager> client_mgr)
    : user_manager(user_mgr),
      file_manager(file_mgr),
      session_manager(session_mgr),
      client_manager(client_mgr) {}

// 根据请求路径和方法调用相应的处理函数，返回HTTP响应
HttpResponse HttpHandler::handle_request(const HttpRequest &request)
{
    //pair<std::string, std::string> route_key = {request.path, request.method};
    //return handlers[route_key](request);
    if (request.path == "/api/register" && request.method == "POST")
        return handle_register(request);
    if (request.path == "/api/login" && request.method == "POST")
        return handle_login(request);
    if (request.path == "/api/logout" && request.method == "POST")
        return handle_logout(request);
    if (request.path == "/api/user/info" && request.method == "GET")
        return handle_user_info(request);
    if (request.path == "/api/user/profile" && (request.method == "PUT" || request.method == "POST"))
        return handle_user_profile_update(request);

    // 文件操作
    if (request.path == "/api/file/list" && request.method == "GET")
        return handle_file_list(request);
    if (request.path == "/api/file/upload/init" && request.method == "POST")
        return handle_file_upload_init(request);
    if (request.path == "/api/file/upload/chunk" && request.method == "POST")
        return handle_file_upload_chunk(request);
    if (request.path == "/api/file/upload/progress" && request.method == "GET")
        return handle_file_upload_progress(request);
    if (request.path == "/api/file/upload/complete" && request.method == "POST")
        return handle_file_upload_complete(request);
    if (request.path == "/api/file/upload/cancel" && request.method == "POST")
        return handle_file_upload_cancel(request);
    if (request.path == "/api/file/upload" && request.method == "POST")
        return handle_file_upload(request);
    if (request.path == "/api/file/download/stream" && request.method == "GET")
        return handle_file_download_stream(request);
    if (request.path.find("/api/file/download") == 0 && request.method == "GET")
        return handle_file_download(request);
    if (request.path == "/api/file/delete" && (request.method == "POST" || request.method == "DELETE"))
        return handle_file_delete(request);
    if (request.path == "/api/file/rename" && (request.method == "POST" || request.method == "PUT"))
        return handle_file_rename(request);
    if (request.path == "/api/file/search" && request.method == "GET")
        return handle_file_search(request);
    if (request.path == "/api/share/create" && request.method == "POST")
        return handle_share_create(request);
    if (request.path.find("/api/share/download") == 0 && request.method == "GET")
        return handle_share_download(request);
    if (request.path == "/api/server/status" && request.method == "GET")
        return handle_server_status(request);
    if (request.path == "/api/message/send" && request.method == "POST")
        return handle_message_send(request);
    if (request.path == "/api/message/list" && request.method == "GET")
        return handle_message_list(request);

    // 静态文件服务：简单的根目录映射
    std::string file_path;
    if (request.path == "/" || request.path == "/index.html")
    {
        file_path = build_static_path(request.path);
    }
    else
    {
        // 简单的安全检查，防止目录遍历攻击
        if (request.path.find("..") != std::string::npos)
        {
            return error_response(403, "Forbidden");
        }

        if (request.path.rfind("/static/", 0) == 0)
        {
            file_path = build_static_path(request.path);
        }
    }

    // 尝试打开文件，如果存在则返回内容，否则返回404
    if (!file_path.empty())
    {
        std::ifstream file(file_path, std::ios::binary);
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

// 构建JSON响应，code是状态码，message是提示信息，data是可选的响应数据（已经是JSON格式字符串）
HttpResponse HttpHandler::json_response(int code, const std::string &message, const std::string &data)
{
    HttpResponse response;
    response.status_code = code;

    // 补上 status_text
    switch (code)
    {
    case 200:
        response.status_text = "OK";
        break;
    case 400:
        response.status_text = "Bad Request";
        break;
    case 401:
        response.status_text = "Unauthorized";
        break;
    case 403:
        response.status_text = "Forbidden";
        break;
    case 404:
        response.status_text = "Not Found";
        break;
    case 410:
        response.status_text = "Gone";
        break;
    case 507:
        response.status_text = "Insufficient Storage";
        break;
    case 500:
        response.status_text = "Internal Server Error";
        break;
    default:
        response.status_text = "Unknown";
        break;
    }

    std::ostringstream ss;
    ss << "{\"code\":" << code << ",\"message\":\"" << message << "\"";
    if (!data.empty())
        ss << ",\"data\":" << data;
    ss << "}";
    response.body = ss.str();
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

    auto user = user_manager->get_user_by_id(user_id);
    if (!user)
    {
        return error_response(404, "User not found");
    }

    JsonBuilder builder;
    builder.add("id", user->id);
    builder.add("username", user->username);
    builder.add("email", user->email);
    builder.add("storage_used", user->storage_used);
    builder.add("storage_limit", user->storage_limit);
    builder.add("created_at", user->created_at);
    builder.add("timeout_minutes", static_cast<int>(session_manager->get_timeout_minutes()));

    return json_response(200, "User info", builder.build());
}

HttpResponse HttpHandler::handle_user_profile_update(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    JsonParser parser(request.body);
    std::string username = parser.get_string("username");
    std::string email = parser.get_string("email");

    if (username.empty() || email.empty())
    {
        return error_response(400, "Username and email are required");
    }

    if (!user_manager->update_profile(user_id, username, email))
    {
        return error_response(400, "Update failed, username or email may already exist");
    }

    auto user = user_manager->get_user_by_id(user_id);
    if (!user)
    {
        return error_response(500, "Failed to load updated profile");
    }

    JsonBuilder builder;
    builder.add("id", user->id);
    builder.add("username", user->username);
    builder.add("email", user->email);
    builder.add("storage_used", user->storage_used);
    builder.add("storage_limit", user->storage_limit);
    builder.add("created_at", user->created_at);
    return json_response(200, "Profile updated", builder.build());
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

HttpResponse HttpHandler::handle_file_upload_init(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    JsonParser parser(request.body);
    std::string filename = parser.get_string("filename");
    long long total_size = parser.get_long("total_size", 0);
    int total_chunks = parser.get_int("total_chunks", 0);
    std::string mime_type = parser.get_string("mime_type", "application/octet-stream");

    if (filename.empty() || total_size <= 0 || total_chunks <= 0)
    {
        return error_response(400, "Invalid upload init parameters");
    }

    auto user = user_manager->get_user_by_id(user_id);
    if (!user)
    {
        return error_response(404, "User not found");
    }
    if (user->storage_used + total_size > user->storage_limit)
    {
        return error_response(507, "Insufficient storage space");
    }

    auto session = file_manager->create_upload_session(
        user_id, filename, total_size, total_chunks, mime_type);
    if (!session)
    {
        return error_response(500, "Failed to initialize upload session");
    }

    const long long chunk_size = (total_size + total_chunks - 1) / total_chunks;
    JsonBuilder builder;
    builder.add("upload_id", session->upload_id);
    builder.add("chunk_size", chunk_size);
    builder.add("total_chunks", total_chunks);
    return json_response(200, "Success", builder.build());
}

HttpResponse HttpHandler::handle_file_upload_chunk(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    std::string upload_id = request.params.count("upload_id") ? request.params.at("upload_id") : "";
    int chunk_index = parse_int_param(request.params, "chunk_index", -1);
    std::string chunk_hash = request.params.count("chunk_hash") ? request.params.at("chunk_hash") : "";

    if (upload_id.empty() || chunk_index < 0 || request.body.empty())
    {
        return error_response(400, "Invalid upload chunk parameters");
    }

    UploadProgress progress = file_manager->get_upload_progress(upload_id, user_id);
    if (!progress.exists)
    {
        return error_response(404, "Upload session not found");
    }
    if (progress.expired)
    {
        return error_response(410, "Upload session expired");
    }
    if (progress.status != "uploading")
    {
        return error_response(400, "Upload session is not active");
    }

    bool ok = file_manager->save_chunk(upload_id, chunk_index, chunk_hash,
                                       request.body.data(), request.body.size());
    if (!ok)
    {
        return error_response(400, "Chunk hash mismatch or save failed");
    }

    return json_response(200, "Chunk uploaded");
}

HttpResponse HttpHandler::handle_file_upload_progress(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    std::string upload_id = request.params.count("upload_id") ? request.params.at("upload_id") : "";
    if (upload_id.empty())
    {
        return error_response(400, "Missing upload_id");
    }

    UploadProgress progress = file_manager->get_upload_progress(upload_id, user_id);
    if (!progress.exists)
    {
        return error_response(404, "Upload session not found");
    }
    if (progress.expired)
    {
        return error_response(410, "Upload session expired");
    }

    std::ostringstream data;
    data << "{"
         << "\"upload_id\":\"" << upload_id << "\","
         << "\"total_chunks\":" << progress.total_chunks << ","
         << "\"completed_chunks\":" << progress.completed_chunks << ","
         << "\"total_size\":" << progress.total_size << ","
         << "\"uploaded_size\":" << progress.uploaded_size << ","
         << "\"progress\":" << std::fixed << std::setprecision(4) << progress.progress
         << "}";
    return json_response(200, "Success", data.str());
}

HttpResponse HttpHandler::handle_file_upload_complete(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    std::string upload_id = request.params.count("upload_id") ? request.params.at("upload_id") : "";
    if (upload_id.empty())
    {
        return error_response(400, "Missing upload_id");
    }

    UploadProgress progress = file_manager->get_upload_progress(upload_id, user_id);
    if (!progress.exists)
    {
        return error_response(404, "Upload session not found");
    }
    if (progress.expired)
    {
        return error_response(410, "Upload session expired");
    }
    if (progress.completed_chunks != progress.total_chunks)
    {
        return error_response(400, "Upload not complete");
    }

    auto result = file_manager->complete_upload(upload_id, user_id);
    if (!result)
    {
        return error_response(500, "Failed to complete upload");
    }

    JsonBuilder builder;
    builder.add("file_id", result->id);
    builder.add("filename", result->original_filename);
    builder.add("size", result->file_size);
    return json_response(200, "Upload complete", builder.build());
}

HttpResponse HttpHandler::handle_file_upload_cancel(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    std::string upload_id = request.params.count("upload_id") ? request.params.at("upload_id") : "";
    if (upload_id.empty())
    {
        return error_response(400, "Missing upload_id");
    }

    UploadProgress progress = file_manager->get_upload_progress(upload_id, user_id);
    if (!progress.exists)
    {
        return error_response(404, "Upload session not found");
    }
    if (progress.expired)
    {
        return error_response(410, "Upload session expired");
    }

    if (!file_manager->cancel_upload(upload_id, user_id))
    {
        return error_response(500, "Failed to cancel upload");
    }
    return json_response(200, "Upload cancelled");
}

HttpResponse HttpHandler::handle_file_upload(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    std::string filename = "uploaded_file";

    auto user = user_manager->get_user_by_id(user_id);
    if (!user)
    {
        return error_response(404, "User not found");
    }

    if (user->storage_used + static_cast<long long>(request.body.size()) > user->storage_limit)
    {
        return error_response(400, "Insufficient storage space");
    }

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

HttpResponse HttpHandler::handle_share_create(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    JsonParser parser(request.body);
    int file_id = parser.get_int("file_id", -1);
    if (file_id == -1)
    {
        return error_response(400, "Missing file_id");
    }

    // 调用 FileManager 生成并保存分享码到数据库
    std::string share_code = file_manager->create_share_code(file_id, user_id);
    if (share_code.empty())
    {
        return error_response(500, "Failed to create share code");
    }

    JsonBuilder builder;
    builder.add("share_code", share_code);
    builder.add("share_url", "/api/share/download?code=" + share_code);
    builder.add("file_id", file_id);
    return json_response(200, "Share created", builder.build());
}

// 通过分享码下载文件的接口，前端访问 /api/share/download?code=xxxx 来下载文件
HttpResponse HttpHandler::handle_share_download(const HttpRequest &request)
{
    std::string code;
    if (request.params.count("code"))
    {
        code = request.params.at("code");
    }

    if (code.empty())
    {
        return error_response(400, "Missing share code");
    }

    // 从数据库获取分享的文件信息
    auto file_info = file_manager->get_shared_file_info(code);
    if (!file_info)
    {
        return error_response(404, "Shared file not found or invalid code");
    }

    // 下载文件内容（传人文件所有者的 user_id）
    auto data = file_manager->download_file(file_info->id, file_info->user_id);
    if (!data)
    {
        return error_response(500, "Failed to download shared file");
    }

    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.headers["Content-Type"] = "application/octet-stream";
    response.headers["Content-Disposition"] = "attachment; filename=\"" + file_info->original_filename + "\"; filename*=UTF-8''" + url_encode_utf8(file_info->original_filename);
    response.body = std::string(data->begin(), data->end());
    return response;
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
        response.headers["Content-Disposition"] = "attachment; filename=\"" + file_info->original_filename + "\"; filename*=UTF-8''" + url_encode_utf8(file_info->original_filename);
        response.body = std::string(data->begin(), data->end());
        return response;
    }
    else
    {
        return error_response(500, "Download failed");
    }
}

HttpResponse HttpHandler::handle_file_download_stream(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
    {
        return error_response(401, "Unauthorized");
    }

    int file_id = parse_int_param(request.params, "id", -1);
    if (file_id <= 0)
    {
        return error_response(400, "Missing file ID");
    }

    auto file_info = file_manager->get_file_info(file_id, user_id);
    if (!file_info)
    {
        return error_response(404, "File not found");
    }

    std::ifstream check(file_info->file_path, std::ios::binary);
    if (!check)
    {
        return error_response(404, "File content not found");
    }
    check.close();

    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.stream_file = true;
    response.stream_file_path = file_info->file_path;
    response.stream_file_size = file_info->file_size;
    response.headers["Content-Type"] = file_info->mime_type.empty()
                                           ? "application/octet-stream"
                                           : file_info->mime_type;
    response.headers["Content-Disposition"] = "attachment; filename=\"" +
                                              file_info->original_filename +
                                              "\"; filename*=UTF-8''" +
                                              url_encode_utf8(file_info->original_filename);
    response.headers["Content-Length"] = std::to_string(file_info->file_size);
    return response;
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
        builder.add("filename", files[i].filename);
        builder.add("original_filename", files[i].original_filename);
        builder.add("size", files[i].file_size);
        builder.add("upload_date", files[i].upload_date);
        json_array += builder.build();
    }
    json_array += "]";

    return json_response(200, "Success", json_array);
}

HttpResponse HttpHandler::handle_server_status(const HttpRequest &request)
{
    (void)request;

    const std::vector<ClientInfo> clients =
        client_manager ? client_manager->get_all_clients() : std::vector<ClientInfo>{};
    const size_t active_connections =
        client_manager ? client_manager->active_count() : 0;

    std::string json_array = "[";
    bool first = true;
    for (const auto &client : clients)
    {
        if (!client.is_active)
        {
            continue;
        }

        if (!first)
        {
            json_array += ",";
        }

        JsonBuilder builder;
        builder.add("socket_fd", client.socket_fd);
        builder.add("ip", client.ip_address);
        builder.add("connect_time", client.connect_time);
        json_array += builder.build();
        first = false;
    }
    json_array += "]";

    std::string data = "{\"active_connections\":" + std::to_string(active_connections) +
                       ",\"clients\":" + json_array + "}";
    return json_response(200, "Server status retrieved successfully", data);
}


// 消息发送接口，前端通过 POST /api/message/send 发送消息，参数包括 receiver_id 和 content
HttpResponse HttpHandler::handle_message_send(const HttpRequest &request)
{
    int sender_id = get_user_id_from_session(request);
    if (sender_id == -1)
        return error_response(401, "Unauthorized");

    JsonParser parser(request.body);
    int receiver_id = parser.get_int("receiver_id", -1);
    std::string content = parser.get_string("content");

    if (receiver_id <= 0 || content.empty())
        return error_response(400, "Invalid parameters");
    if (receiver_id == sender_id)
        return error_response(400, "Cannot send message to yourself");

    auto receiver = user_manager->get_user_by_id(receiver_id);
    if (!receiver)
        return error_response(400, "Receiver not found");

    if (!user_manager->send_message(sender_id, receiver_id, content))
        return error_response(500, "Failed to send message");

    return json_response(200, "Message sent");
}

HttpResponse HttpHandler::handle_message_list(const HttpRequest &request)
{
    int user_id = get_user_id_from_session(request);
    if (user_id == -1)
        return error_response(401, "Unauthorized");

    int with_user_id = parse_int_param(request.params, "with_user_id", -1);
    int limit = parse_int_param(request.params, "limit", 50);
    if (with_user_id <= 0)
        return error_response(400, "with_user_id required");
    if (limit <= 0)
        limit = 50;
    if (limit > 100)
        limit = 100;

    auto with_user = user_manager->get_user_by_id(with_user_id);
    if (!with_user)
        return error_response(400, "Conversation user not found");

    std::vector<Message> messages = user_manager->get_messages(user_id, with_user_id, limit);
    user_manager->mark_messages_read(user_id, with_user_id);

    std::string json_array = "[";
    for (size_t i = 0; i < messages.size(); ++i)
    {
        if (i > 0)
            json_array += ",";
        JsonBuilder builder;
        builder.add("id", messages[i].id);
        builder.add("sender_id", messages[i].sender_id);
        builder.add("receiver_id", messages[i].receiver_id);
        builder.add("content", messages[i].content);
        builder.add("is_read", messages[i].is_read);
        builder.add("created_at", messages[i].created_at);
        json_array += builder.build();
    }
    json_array += "]";

    return json_response(200, "ok", json_array);
}

// 生成分享码和下载分享文件的接口被移除，功能已合并到 HttpHandler 成员函数中

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
    if (request.headers.count("X-Token"))
    {
        return request.headers.at("X-Token");
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

