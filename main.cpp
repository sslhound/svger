#include <stdio.h>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "spdlog/spdlog.h"
#include "CLI/CLI.hpp"

using namespace std;
using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

class Server
{
public:
    Server() {}
    Server(utility::string_t url, int max_size);

    pplx::task<void> open() { return m_listener.open(); }
    pplx::task<void> close() { return m_listener.close(); }

private:
    void handle_get(http_request message);
    void handle_put(http_request message);
    void handle_post(http_request message);
    void handle_delete(http_request message);

    http_listener m_listener;
};

std::unique_ptr<Server> g_httpServer;

void on_initialize(const string_t &address, int max_size)
{
    uri_builder uri(address);
    uri.append_path(U("/"));

    utility::string_t addr = uri.to_uri().to_string();
    g_httpServer = std::unique_ptr<Server>(new Server(addr, max_size));
    g_httpServer->open().wait();

    spdlog::info("Started server on {}", addr);

    return;
}

void on_shutdown()
{
    g_httpServer->close().wait();
    return;
}

int main(int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    CLI::App app{"svger"};
    utility::string_t listen = "http://0.0.0.0:5003";
    std::string environment = "development";
    int max_size = 2 * 1048576;
    app.add_option("--listen", listen, "Listen on a specific interface and port.")->envname("LISTEN");
    app.add_option("--environment", environment, "The environment.")->envname("ENVIRONMENT");
    app.add_option("--max-size", max_size, "The max SVG size to process.")->envname("MAX_SIZE");

    CLI11_PARSE(app, argc, argv);

    if (environment == "production")
    {
        spdlog::set_level(spdlog::level::info);
    }

    spdlog::info("svger starting up environment={} listen={}", environment, listen);

    utility::string_t address = U("http://");
    address.append(listen);

    on_initialize(address, max_size);
    std::cout << "Press ENTER to exit." << std::endl;

    std::string line;
    std::getline(std::cin, line);

    on_shutdown();
    return 0;
}