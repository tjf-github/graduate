#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include "http_handler.h"

class HttpParser
{
public:
    static HttpRequest parse(const std::string &raw_request);
    static std::string build_response(const HttpResponse &response);
};

#endif
