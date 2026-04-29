#include <arpa/inet.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <netdb.h>
#include <numeric>
#include <regex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
struct HttpResult
{
    int status_code = 0;
    std::string headers;
    std::string body;
    double elapsed_ms = 0.0;
};

struct ScenarioStats
{
    std::string name;
    int concurrency = 0;
    int success_count = 0;
    int total_count = 0;
    double average_ms = 0.0;
    double total_ms = 0.0;
};

class Barrier
{
public:
    explicit Barrier(int count) : target_(count), current_(0), released_(false) {}

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++current_;
        if (current_ >= target_)
        {
            released_ = true;
            cv_.notify_all();
            return;
        }
        cv_.wait(lock, [&] { return released_; });
    }

private:
    int target_;
    int current_;
    bool released_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

std::string get_env_or_default(const char *key, const std::string &fallback)
{
    const char *value = std::getenv(key);
    return value ? std::string(value) : fallback;
}

std::string json_escape(const std::string &input)
{
    std::ostringstream oss;
    for (char c : input)
    {
        switch (c)
        {
        case '\\':
            oss << "\\\\";
            break;
        case '"':
            oss << "\\\"";
            break;
        case '\n':
            oss << "\\n";
            break;
        case '\r':
            oss << "\\r";
            break;
        case '\t':
            oss << "\\t";
            break;
        default:
            oss << c;
            break;
        }
    }
    return oss.str();
}

std::string extract_json_string(const std::string &body, const std::string &key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(body, match, pattern) && match.size() > 1)
    {
        return match[1].str();
    }
    return "";
}

int extract_json_int(const std::string &body, const std::string &key)
{
    const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(body, match, pattern) && match.size() > 1)
    {
        return std::stoi(match[1].str());
    }
    return -1;
}

HttpResult send_http_request(const std::string &host,
                             int port,
                             const std::string &method,
                             const std::string &path,
                             const std::vector<std::string> &headers,
                             const std::string &body)
{
    HttpResult result;
    const auto start = std::chrono::steady_clock::now();

    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return result;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
    {
        ::close(sock);
        return result;
    }

    if (::connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        ::close(sock);
        return result;
    }

    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << ":" << port << "\r\n";
    request << "Connection: close\r\n";
    for (const auto &header : headers)
    {
        request << header << "\r\n";
    }
    request << "Content-Length: " << body.size() << "\r\n\r\n";

    const std::string head = request.str();
    std::vector<char> payload(head.begin(), head.end());
    payload.insert(payload.end(), body.begin(), body.end());

    std::size_t sent = 0;
    while (sent < payload.size())
    {
        const ssize_t n = ::send(sock, payload.data() + sent, payload.size() - sent, 0);
        if (n <= 0)
        {
            ::close(sock);
            return result;
        }
        sent += static_cast<std::size_t>(n);
    }

    std::string response;
    char buffer[8192];
    while (true)
    {
        const ssize_t n = ::recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0)
        {
            break;
        }
        response.append(buffer, buffer + n);
    }
    ::close(sock);

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    const std::size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos)
    {
        return result;
    }

    result.headers = response.substr(0, header_end);
    result.body = response.substr(header_end + 4);

    std::istringstream status_line_stream(result.headers);
    std::string http_version;
    status_line_stream >> http_version >> result.status_code;
    return result;
}

bool ensure_ok(const HttpResult &result)
{
    return result.status_code == 200;
}

bool register_user(const std::string &host,
                   int port,
                   const std::string &username,
                   const std::string &email,
                   const std::string &password)
{
    const std::string body =
        "{\"username\":\"" + json_escape(username) +
        "\",\"email\":\"" + json_escape(email) +
        "\",\"password\":\"" + json_escape(password) + "\"}";

    const HttpResult result = send_http_request(
        host,
        port,
        "POST",
        "/api/register",
        std::vector<std::string>{"Content-Type: application/json"},
        body);

    return result.status_code == 200 || result.status_code == 400;
}

bool login_user(const std::string &host,
                int port,
                const std::string &username,
                const std::string &password,
                std::string &token,
                int &user_id,
                double *elapsed_ms = nullptr)
{
    const std::string body =
        "{\"username\":\"" + json_escape(username) +
        "\",\"password\":\"" + json_escape(password) + "\"}";

    const HttpResult result = send_http_request(
        host,
        port,
        "POST",
        "/api/login",
        std::vector<std::string>{"Content-Type: application/json"},
        body);

    if (elapsed_ms)
    {
        *elapsed_ms = result.elapsed_ms;
    }
    if (!ensure_ok(result))
    {
        return false;
    }

    token = extract_json_string(result.body, "token");
    user_id = extract_json_int(result.body, "id");
    return !token.empty() && user_id > 0;
}

bool upload_file(const std::string &host,
                 int port,
                 const std::string &token,
                 const std::string &filename,
                 const std::string &content,
                 double &elapsed_ms)
{
    const HttpResult result = send_http_request(
        host,
        port,
        "POST",
        "/api/file/upload",
        std::vector<std::string>{
            "Authorization: Bearer " + token,
            "Content-Type: application/octet-stream",
            "X-Filename: " + filename},
        content);
    elapsed_ms = result.elapsed_ms;
    return ensure_ok(result);
}

bool list_files(const std::string &host, int port, const std::string &token, double &elapsed_ms)
{
    const HttpResult result = send_http_request(
        host,
        port,
        "GET",
        "/api/file/list?offset=0&limit=10",
        std::vector<std::string>{"Authorization: Bearer " + token},
        "");
    elapsed_ms = result.elapsed_ms;
    return ensure_ok(result);
}

bool send_message(const std::string &host,
                  int port,
                  const std::string &token,
                  int receiver_id,
                  const std::string &content,
                  double &elapsed_ms)
{
    const std::string body =
        "{\"receiver_id\":" + std::to_string(receiver_id) +
        ",\"content\":\"" + json_escape(content) + "\"}";
    const HttpResult result = send_http_request(
        host,
        port,
        "POST",
        "/api/message/send",
        std::vector<std::string>{
            "Authorization: Bearer " + token,
            "Content-Type: application/json"},
        body);
    elapsed_ms = result.elapsed_ms;
    return ensure_ok(result);
}

bool list_messages(const std::string &host,
                   int port,
                   const std::string &token,
                   int with_user_id,
                   double &elapsed_ms)
{
    const std::string path =
        "/api/message/list?with_user_id=" + std::to_string(with_user_id) + "&limit=50";
    const HttpResult result = send_http_request(
        host,
        port,
        "GET",
        path,
        std::vector<std::string>{"Authorization: Bearer " + token},
        "");
    elapsed_ms = result.elapsed_ms;
    return ensure_ok(result);
}

ScenarioStats build_stats(const std::string &name,
                          int concurrency,
                          const std::vector<int> &success_flags,
                          const std::vector<double> &elapsed_ms,
                          double total_ms)
{
    ScenarioStats stats;
    stats.name = name;
    stats.concurrency = concurrency;
    stats.total_count = static_cast<int>(success_flags.size());
    stats.success_count = std::accumulate(success_flags.begin(), success_flags.end(), 0);
    stats.total_ms = total_ms;
    if (!elapsed_ms.empty())
    {
        const double sum = std::accumulate(elapsed_ms.begin(), elapsed_ms.end(), 0.0);
        stats.average_ms = sum / static_cast<double>(elapsed_ms.size());
    }
    return stats;
}

void print_stats(const ScenarioStats &stats)
{
    const double success_rate =
        stats.total_count == 0 ? 0.0 : (100.0 * stats.success_count / stats.total_count);
    std::cout << std::left << std::setw(14) << stats.name << " | "
              << std::setw(6) << stats.concurrency << " | "
              << std::setw(10) << std::fixed << std::setprecision(2) << success_rate << " | "
              << std::setw(16) << std::fixed << std::setprecision(2) << stats.average_ms << " | "
              << std::fixed << std::setprecision(2) << stats.total_ms << "\n";
}
} // namespace

int main()
{
    const std::string host = "127.0.0.1";
    const int port = std::atoi(get_env_or_default("PORT", "8080").c_str());
    const auto seed = static_cast<long long>(std::chrono::system_clock::now().time_since_epoch().count());

    std::ostringstream user_a_builder;
    user_a_builder << "concurrent_user_a_" << seed;
    std::ostringstream user_b_builder;
    user_b_builder << "concurrent_user_b_" << seed;

    const std::string username_a = user_a_builder.str();
    const std::string username_b = user_b_builder.str();
    const std::string email_a = username_a + "@example.com";
    const std::string email_b = username_b + "@example.com";
    const std::string password = "Passw0rd!123";

    if (!register_user(host, port, username_a, email_a, password) ||
        !register_user(host, port, username_b, email_b, password))
    {
        std::cerr << "Failed to create test users\n";
        return 1;
    }

    std::string token_a;
    std::string token_b;
    int user_id_a = -1;
    int user_id_b = -1;
    if (!login_user(host, port, username_a, password, token_a, user_id_a) ||
        !login_user(host, port, username_b, password, token_b, user_id_b))
    {
        std::cerr << "Initial login failed\n";
        return 1;
    }

    std::vector<ScenarioStats> all_stats;

    {
        const int concurrency = 10;
        Barrier barrier(concurrency);
        std::vector<int> success(concurrency, 0);
        std::vector<double> elapsed(concurrency, 0.0);
        std::vector<std::thread> threads;

        const auto total_start = std::chrono::steady_clock::now();
        for (int i = 0; i < concurrency; ++i)
        {
            threads.emplace_back([&, i] {
                barrier.wait();
                std::string token;
                int user_id = -1;
                success[i] = login_user(host, port, username_a, password, token, user_id, &elapsed[i]) ? 1 : 0;
            });
        }
        for (auto &thread : threads)
        {
            thread.join();
        }
        const auto total_end = std::chrono::steady_clock::now();
        const double total_ms =
            std::chrono::duration<double, std::milli>(total_end - total_start).count();
        all_stats.push_back(build_stats("A-login", concurrency, success, elapsed, total_ms));
    }

    {
        const int concurrency = 50;
        Barrier barrier(concurrency);
        std::vector<int> success(concurrency, 0);
        std::vector<double> elapsed(concurrency, 0.0);
        std::vector<std::thread> threads;

        const auto total_start = std::chrono::steady_clock::now();
        for (int i = 0; i < concurrency; ++i)
        {
            threads.emplace_back([&, i] {
                barrier.wait();
                success[i] = list_files(host, port, token_a, elapsed[i]) ? 1 : 0;
            });
        }
        for (auto &thread : threads)
        {
            thread.join();
        }
        const auto total_end = std::chrono::steady_clock::now();
        const double total_ms =
            std::chrono::duration<double, std::milli>(total_end - total_start).count();
        all_stats.push_back(build_stats("B-files", concurrency, success, elapsed, total_ms));
    }

    {
        const int concurrency = 20;
        const std::string payload(10 * 1024 * 1024, 'Z');
        Barrier barrier(concurrency);
        std::vector<int> success(concurrency, 0);
        std::vector<double> elapsed(concurrency, 0.0);
        std::vector<std::string> login_tokens(concurrency);
        std::vector<std::thread> threads;

        for (int i = 0; i < concurrency; ++i)
        {
            int logged_user_id = -1;
            if (!login_user(host, port, username_a, password, login_tokens[i], logged_user_id))
            {
                std::cerr << "Scenario C login preparation failed\n";
                return 1;
            }
        }

        const auto total_start = std::chrono::steady_clock::now();
        for (int i = 0; i < concurrency; ++i)
        {
            threads.emplace_back([&, i] {
                barrier.wait();
                std::ostringstream filename;
                filename << "concurrent_upload_" << i << ".bin";
                success[i] = upload_file(host, port, login_tokens[i], filename.str(), payload, elapsed[i]) ? 1 : 0;
            });
        }
        for (auto &thread : threads)
        {
            thread.join();
        }
        const auto total_end = std::chrono::steady_clock::now();
        const double total_ms =
            std::chrono::duration<double, std::milli>(total_end - total_start).count();
        all_stats.push_back(build_stats("C-upload", concurrency, success, elapsed, total_ms));
    }

    {
        const int sender_count = 50;
        const int list_count = 50;
        const int total_threads = sender_count + list_count;
        Barrier barrier(total_threads);
        std::vector<int> send_success(sender_count, 0);
        std::vector<int> list_success(list_count, 0);
        std::vector<double> send_elapsed(sender_count, 0.0);
        std::vector<double> list_elapsed(list_count, 0.0);
        std::vector<std::thread> threads;

        const auto total_start = std::chrono::steady_clock::now();
        for (int i = 0; i < sender_count; ++i)
        {
            threads.emplace_back([&, i] {
                barrier.wait();
                std::ostringstream content;
                content << "parallel-message-" << i;
                send_success[i] =
                    send_message(host, port, token_a, user_id_b, content.str(), send_elapsed[i]) ? 1 : 0;
            });
        }
        for (int i = 0; i < list_count; ++i)
        {
            threads.emplace_back([&, i] {
                barrier.wait();
                list_success[i] = list_messages(host, port, token_b, user_id_a, list_elapsed[i]) ? 1 : 0;
            });
        }
        for (auto &thread : threads)
        {
            thread.join();
        }
        const auto total_end = std::chrono::steady_clock::now();
        const double total_ms =
            std::chrono::duration<double, std::milli>(total_end - total_start).count();
        all_stats.push_back(build_stats("D-send", sender_count, send_success, send_elapsed, total_ms));
        all_stats.push_back(build_stats("D-list", list_count, list_success, list_elapsed, total_ms));
    }

    std::cout << "场景           | 并发数 | 成功率(%)  | 平均响应时间(ms) | 总耗时(ms)\n";
    std::cout << "-----------------------------------------------------------------------\n";
    for (const auto &stats : all_stats)
    {
        print_stats(stats);
    }

    return 0;
}
