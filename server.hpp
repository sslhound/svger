#include <stdio.h>
#include <signal.h>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"
#include <prometheus/registry.h>
#include <prometheus/collectable.h>
#include <prometheus/counter.h>
#include <prometheus/summary.h>

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;
using namespace prometheus;

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
    // Server() {}
    Server(ServerConfig config);

    pplx::task<void> open() { return m_listener.open(); }
    pplx::task<void> close() { return m_listener.close(); }

private:
    void handle_get(http_request message);
    void handle_post(http_request message);

    std::vector<MetricFamily> CollectMetrics() const;

    http_listener m_listener;
    ServerConfig config;
    std::shared_ptr<Registry> registry;
    std::vector<std::weak_ptr<Collectable>> collectables;

    Family<Counter> &server_bytes_transferred_family_;
    Counter &server_bytes_transferred_;

    Family<Summary> &server_get_latencies_family_;
    Summary &server_get_latencies_;

    Family<Summary> &server_post_latencies_family_;
    Summary &server_post_latencies_;

    Family<Counter> &server_get_count_family_;
    Counter &server_get_count_;

    Family<Counter> &server_post_count_family_;
    Counter &server_post_count_;

    Family<Counter> &client_bytes_transferred_family_;
    Counter &client_bytes_transferred_;

    Family<Counter> &client_request_total_family_;
    Counter &client_request_total_;

    Family<Summary> &client_latencies_family_;
    Summary &client_latencies_;

    Family<Summary> &render_latencies_family_;
    Summary &render_latencies_;
};
