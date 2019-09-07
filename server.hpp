#include <stdio.h>
#include <signal.h>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

class ServerConfig
{
public:
    utility::string_t addr;
    bool enable_get;
    bool enable_post;
    int max_size;
    utility::string_t backend;
    bool backend_insecure;
};

class Server
{
public:
    Server() {}
    Server(ServerConfig config);

    pplx::task<void> open() { return m_listener.open(); }
    pplx::task<void> close() { return m_listener.close(); }

private:
    void handle_get(http_request message);
    void handle_post(http_request message);

    http_listener m_listener;
    ServerConfig config;
};
