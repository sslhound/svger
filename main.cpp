// (c) 2019 SSL Hound
// This code is licensed under MIT license (see LICENSE for details)

#include <stdio.h>
#include <signal.h>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "spdlog/spdlog.h"
#include "CLI/CLI.hpp"
#include "server.hpp"

using namespace std;
using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

std::unique_ptr<Server> g_httpServer;

void on_initialize(const ServerConfig config)
{
    g_httpServer = std::unique_ptr<Server>(new Server(config));
    g_httpServer->open().wait();

    spdlog::info("Started server on {}", config.addr);

    return;
}

void on_shutdown()
{
    g_httpServer->close().wait();
    return;
}

void signalHandler(int signum)
{
    spdlog::info("Interrupt signal received: {}", signum);

    on_shutdown();

    exit(signum);
}

int main(int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    CLI::App app{"svger"};
    std::string listen = "0.0.0.0";
    int port = 5003;
    std::string environment = "development";
    std::string backend = "";
    int max_size = 1048576;
    bool backend_insecure = false;
    bool enable_get = true;
    bool enable_post = true;
    app.add_option("--listen", listen, "Listen on a specific interface and port.")->envname("LISTEN")->capture_default_str();
    app.add_option("--port", port, "Listen on a specific port.")->envname("PORT")->capture_default_str();
    app.add_option("--environment", environment, "The environment.")->envname("ENVIRONMENT")->capture_default_str();
    app.add_option("--backend", backend, "The backend to render from")->envname("BACKEND")->capture_default_str();
    app.add_option("--backend-insecure", backend_insecure, "Ignore backend SSL validation.")->envname("BACKEND_INSECURE")->capture_default_str();
    app.add_option("--max-size", max_size, "The max SVG size to process.")->envname("MAX_SIZE")->capture_default_str();
    app.add_option("--enable-get", enable_get, "Enable to GET method.")->envname("ENABLE_GET")->capture_default_str();
    app.add_option("--enable-post", enable_post, "Enable the POST method.")->envname("ENABLE_POST")->capture_default_str();

    CLI11_PARSE(app, argc, argv);

    if (environment == "production")
    {
        spdlog::set_level(spdlog::level::info);
    }

    if (max_size <= 0)
    {
        spdlog::critical("invalid option: max_size must be greater than 0");
        return 1;
    }

    if (!enable_get && !enable_post)
    {
        spdlog::critical("invalid option: at least one of enable-get and enable-post must be true");
        return 1;
    }

    spdlog::info("svger starting up environment={} listen={} backend={}", environment, listen, backend);

    uri_builder addr_builder("http://" + listen);
    addr_builder.set_port(port);
    addr_builder.set_path(U("/"));

    ServerConfig config;
    config.addr = addr_builder.to_uri().to_string();
    config.max_size = max_size;
    config.enable_get = enable_get;
    config.enable_post = enable_post;
    config.backend = backend;
    config.backend_insecure = backend_insecure;

    on_initialize(config);

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    while (1)
    {
        sleep(1);
    }

    return 0;
}