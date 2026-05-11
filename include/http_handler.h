#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include "user_manager.h"
#include "file_manager.h"
#include "session_manager.h"
#include "client_manager.h"

// HTTP 请求对象
struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> params;
};

// HTTP 响应对象
struct HttpResponse
{
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;
    bool stream_file = false;
    std::string stream_file_path;
    long long stream_file_size = 0;
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
                std::shared_ptr<SessionManager> session_mgr,
                std::shared_ptr<ClientManager> client_mgr);

    // 路由入口：按 method + path 分发到对应处理函数
    HttpResponse handle_request(const HttpRequest &request);

private:
    std::shared_ptr<UserManager> user_manager;
    std::shared_ptr<FileManager> file_manager;
    std::shared_ptr<SessionManager> session_manager;
    std::shared_ptr<ClientManager> client_manager;

    HttpResponse handle_register(const HttpRequest &request);
    HttpResponse handle_login(const HttpRequest &request);
    HttpResponse handle_logout(const HttpRequest &request);
    HttpResponse handle_user_info(const HttpRequest &request);
    HttpResponse handle_user_profile_update(const HttpRequest &request);
    HttpResponse handle_file_upload(const HttpRequest &request);
    HttpResponse handle_file_upload_init(const HttpRequest &request);
    HttpResponse handle_file_upload_chunk(const HttpRequest &request);
    HttpResponse handle_file_upload_progress(const HttpRequest &request);
    HttpResponse handle_file_upload_complete(const HttpRequest &request);
    HttpResponse handle_file_upload_cancel(const HttpRequest &request);
    HttpResponse handle_file_download(const HttpRequest &request);
    HttpResponse handle_file_download_stream(const HttpRequest &request);
    HttpResponse handle_file_list(const HttpRequest &request);
    HttpResponse handle_file_delete(const HttpRequest &request);
    HttpResponse handle_file_rename(const HttpRequest &request);
    HttpResponse handle_file_search(const HttpRequest &request);
    HttpResponse handle_share_create(const HttpRequest &request);
    HttpResponse handle_share_download(const HttpRequest &request);
    HttpResponse handle_server_status(const HttpRequest &request);

    HttpResponse handle_message_send(const HttpRequest &request);
    HttpResponse handle_message_list(const HttpRequest &request);

    int get_user_id_from_session(const HttpRequest &request);
    std::string get_session_token(const HttpRequest &request);
    HttpResponse json_response(int code, const std::string &message,
                               const std::string &data = "");
    HttpResponse error_response(int code, const std::string &message);
    HttpResponse error_response_with_context(const HttpRequest &request,
                                             int code,
                                             const std::string &message,
                                             const std::string &reason,
                                             int user_id = -1);
};


#endif
