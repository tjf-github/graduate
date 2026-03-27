#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include "user_manager.h"
#include "file_manager.h"
#include "session_manager.h"

// http请求的基本结构体，包含请求方法、路径、版本、头部信息、请求体和查询参数等
struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> params;
};

// http响应的基本结构体，包含状态码、状态文本、头部信息和响应体等
struct HttpResponse
{
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;
    HttpResponse() : status_code(200), status_text("OK")
    {
        headers["Content-Type"] = "application/json";
    }
};

class HttpHandler
{
public:
    HttpHandler(std::shared_ptr<UserManager> user_mgr,
                std::shared_ptr<FileManager> file_mgr,
                std::shared_ptr<SessionManager> session_mgr);

    // 处理HTTP请求，根据请求路径和方法调用相应的处理函数，返回HTTP响应
    HttpResponse handle_request(const HttpRequest &request);

private:
    struct ShareInfo
    {
        std::string share_code;
        int owner_user_id;
        int file_id;
        std::string created_at;
    };

    std::shared_ptr<UserManager> user_manager;
    std::shared_ptr<FileManager> file_manager;
    std::shared_ptr<SessionManager> session_manager;

    HttpResponse handle_register(const HttpRequest &request);
    HttpResponse handle_login(const HttpRequest &request);
    HttpResponse handle_logout(const HttpRequest &request);
    HttpResponse handle_user_info(const HttpRequest &request);
    HttpResponse handle_user_profile_update(const HttpRequest &request);
    HttpResponse handle_file_upload(const HttpRequest &request);
    HttpResponse handle_file_download(const HttpRequest &request);
    HttpResponse handle_file_list(const HttpRequest &request);
    HttpResponse handle_file_delete(const HttpRequest &request);
    HttpResponse handle_file_rename(const HttpRequest &request);
    HttpResponse handle_file_search(const HttpRequest &request);
    HttpResponse handle_share_create(const HttpRequest &request);
    HttpResponse handle_share_download(const HttpRequest &request);

    HttpResponse handle_message_send(const HttpRequest &request);
    HttpResponse handle_message_list(const HttpRequest &request);

    int get_user_id_from_session(const HttpRequest &request);
    std::string get_session_token(const HttpRequest &request);
    HttpResponse json_response(int code, const std::string &message,
                               const std::string &data = "");
    HttpResponse error_response(int code, const std::string &message);
};

// 简单的HTTP请求解析器和响应构建器，支持基本的HTTP协议格式
class HttpParser
{
public:
    static HttpRequest parse(const std::string &raw_request);
    static std::string build_response(const HttpResponse &response);
};

#endif
