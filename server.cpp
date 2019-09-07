// (c) 2019 SSL Hound
// This code is licensed under MIT license (see LICENSE for details)

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fstream>
#include <algorithm>
#include <random>
#include <sstream>
#include <vector>
#include <chrono>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/http_client.h"
#include "spdlog/spdlog.h"
#include "server.hpp"
#include <prometheus/registry.h>
#include <prometheus/collectable.h>
#include <prometheus/counter.h>
#include "prometheus/serializer.h"
#include "prometheus/text_serializer.h"

using namespace std;
using namespace std::chrono;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;
using namespace web::http;
using namespace web::http::client;
using namespace prometheus;

static _cairo_status cairoWriteFunc(void *context, const unsigned char *data, unsigned int length)
{
    auto outData = static_cast<std::vector<uint8_t> *>(context);
    auto offset = static_cast<unsigned int>(outData->size());
    outData->resize(offset + length);
    memcpy(outData->data() + offset, data, length);
    return CAIRO_STATUS_SUCCESS;
}

Server::Server(ServerConfig config) : m_listener(config.addr),
                                      config(config),
                                      registry(std::make_shared<prometheus::Registry>()),

                                      server_bytes_transferred_family_(
                                          BuildCounter()
                                              .Name("server_transferred_bytes_total")
                                              .Help("Transferred bytes to from svger")
                                              .Register(*registry)),
                                      server_bytes_transferred_(server_bytes_transferred_family_.Add({{"application", "svger"}})),

                                      server_get_latencies_family_(
                                          BuildSummary()
                                              .Name("server_get_latencies")
                                              .Help("Latencies of serving svger GET requests, in microseconds")
                                              .Register(*registry)),
                                      server_get_latencies_(server_get_latencies_family_.Add({{"application", "svger"}}, Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}})),

                                      server_post_latencies_family_(
                                          BuildSummary()
                                              .Name("server_post_latencies")
                                              .Help("Latencies of serving svger POST requests, in microseconds")
                                              .Register(*registry)),
                                      server_post_latencies_(server_post_latencies_family_.Add({{"application", "svger"}}, Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}})),

                                      server_get_count_family_(
                                          BuildCounter()
                                              .Name("server_get_total")
                                              .Help("Number of times a GET request was made")
                                              .Register(*registry)),
                                      server_get_count_(server_get_count_family_.Add({{"application", "svger"}})),

                                      server_post_count_family_(
                                          BuildCounter()
                                              .Name("backend_post_total")
                                              .Help("Number of times a POST request was made")
                                              .Register(*registry)),
                                      server_post_count_(server_post_count_family_.Add({{"application", "svger"}})),

                                      client_bytes_transferred_family_(
                                          BuildCounter()
                                              .Name("backend_bytes_transferred_total")
                                              .Help("Transferred bytes to from backend")
                                              .Register(*registry)),
                                      client_bytes_transferred_(client_bytes_transferred_family_.Add({{"application", "svger"}})),

                                      client_request_total_family_(
                                          BuildCounter()
                                              .Name("backend_post_total")
                                              .Help("Number of times a backend request was made")
                                              .Register(*registry)),
                                      client_request_total_(client_request_total_family_.Add({{"application", "svger"}})),

                                      client_latencies_family_(
                                          BuildSummary()
                                              .Name("backend_request_latencies")
                                              .Help("Latencies of backend requests, in microseconds")
                                              .Register(*registry)),
                                      client_latencies_(client_latencies_family_.Add({{"application", "svger"}}, Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}})),

                                      render_latencies_family_(
                                          BuildSummary()
                                              .Name("render_latencies")
                                              .Help("Latencies of render work, in microseconds")
                                              .Register(*registry)),
                                      render_latencies_(render_latencies_family_.Add({{"application", "svger"}}, Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))

{
    if (config.enable_get)
    {
        m_listener.support(methods::GET, std::bind(&Server::handle_get, this, std::placeholders::_1));
    }
    if (config.enable_post)
    {
        m_listener.support(methods::POST, std::bind(&Server::handle_post, this, std::placeholders::_1));
    }
    collectables.push_back(registry);
}

void Server::handle_get(http_request message)
{
    auto request_start = std::chrono::steady_clock::now();
    server_get_count_.Increment();
    auto path = message.relative_uri().path();
    spdlog::debug("get request {}", message.to_string());

    if (path == "/")
    {
        message.reply(status_codes::OK, U("OK"));
        return;
    }

    if (path == "/metrics")
    {
        auto metrics = CollectMetrics();

        auto serializer = std::unique_ptr<Serializer>{new TextSerializer()};
        auto metrics_body = serializer->Serialize(metrics);

        server_bytes_transferred_.Increment(metrics_body.size());

        message.reply(status_codes::OK, metrics_body);
        return;
    }

    spdlog::info("Preparing to connect to {}", config.backend + path);

    http_client_config proxy_client_config;
    proxy_client_config.set_validate_certificates(!config.backend_insecure);

    http_request proxy_request(methods::GET);

    auto headers = message.headers();
    utility::string_t if_modified_since;
    if (headers.match(U("If-Modified-Since"), if_modified_since))
    {
        proxy_request.headers().add(U("If-Modified-Since"), if_modified_since);
    }
    utility::string_t if_unmodified_since;
    if (headers.match(U("If-Unmodified-Since"), if_unmodified_since))
    {
        proxy_request.headers().add(U("If-Unmodified-since"), if_unmodified_since);
    }
    utility::string_t if_none_match;
    if (headers.match(U("If-None-Match"), if_none_match))
    {
        proxy_request.headers().add(U("If-None-Match"), if_none_match);
    }

    http_client proxy_client(config.backend + path, proxy_client_config);

    http_response proxy_response;

    client_request_total_.Increment();
    auto proxy_request_start = std::chrono::steady_clock::now();
    try
    {
        proxy_response = proxy_client.request(proxy_request).get();
    }
    catch (const std::exception &e)
    {
        spdlog::error("error doing stuff: {}", e.what());
        message.reply(status_codes::InternalError);
        return;
    }
    auto proxy_request_stop = std::chrono::steady_clock::now();
    auto proxy_request_duration = std::chrono::duration_cast<std::chrono::microseconds>(proxy_request_stop - proxy_request_start);
    client_latencies_.Observe(proxy_request_duration.count());

    spdlog::debug("proxy response {}", proxy_response.to_string());

    if (proxy_response.status_code() == 304)
    {
        message.reply(status_codes::NotModified);
        return;
    }

    if (proxy_response.status_code() != 200)
    {
        message.reply(proxy_response.status_code());
        return;
    }

    client_bytes_transferred_.Increment(proxy_response.headers().content_length());

    web::http::http_response response(status_codes::OK);

    auto response_headers = proxy_response.headers();
    utility::string_t cache_control;
    if (response_headers.match(U("Cache-Control"), cache_control))
    {
        response.headers().add(U("Cache-Control"), cache_control);
    }
    utility::string_t last_modified;
    if (response_headers.match(U("Last-Modified"), last_modified))
    {
        response.headers().add(U("Last-Modified"), last_modified);
    }

    utility::string_t date_header;
    if (response_headers.match(U("Date"), date_header))
    {
        response.headers().add(U("Date"), date_header);
    }

    auto body = proxy_response.extract_vector().get();
    spdlog::debug("proxy_response body {}", body.data());

    GError *error = NULL;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    double width, height;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t status;

    auto render_start = std::chrono::steady_clock::now();
    handle = rsvg_handle_new_from_data((const guint8 *)reinterpret_cast<char *>(body.data()), (gsize)body.size(), &error);
    if (error != NULL)
    {
        spdlog::error("rsvg_handle_new_from_file error {}", error->message);
        message.reply(status_codes::InternalError, U("unable to parse payload"));
        g_error_free(error);
        return;
    }

    rsvg_handle_get_dimensions(handle, &dim);
    width = dim.width;
    height = dim.height;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    cr = cairo_create(surface);

    rsvg_handle_render_cairo(handle, cr);

    status = cairo_status(cr);
    if (status)
    {
        spdlog::error("cairo_status error {}", cairo_status_to_string(status));
        message.reply(status_codes::InternalError, U(cairo_status_to_string(status)));
        return;
    }

    std::vector<unsigned char> out;
    status = cairo_surface_write_to_png_stream(surface, cairoWriteFunc, &out);
    if (status)
    {
        spdlog::error("cairo_status error {}", cairo_status_to_string(status));
        message.reply(status_codes::InternalError, U(cairo_status_to_string(status)));
        return;
    }

    auto render_stop = std::chrono::steady_clock::now();
    auto render_duration = std::chrono::duration_cast<std::chrono::microseconds>(render_stop - render_start);
    render_latencies_.Observe(render_duration.count());

    response.headers().set_content_type(U("image/png"));
    response.set_body(out);
    message.reply(response);

    server_bytes_transferred_.Increment(out.size());

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);

    auto request_stop = std::chrono::steady_clock::now();
    auto request_stop_duration = std::chrono::duration_cast<std::chrono::microseconds>(request_stop - request_start);
    server_get_latencies_.Observe(request_stop_duration.count());
};

void Server::handle_post(http_request message)
{
    auto request_start = std::chrono::steady_clock::now();
    server_post_count_.Increment();
    auto headers = message.headers();

    if (!headers.has("Content-Type"))
    {
        spdlog::error("client did not provide a content type");
        message.reply(status_codes::UnsupportedMediaType, U("Unsupported Media Type"));
        return;
    }
    if (!headers.has("Content-Length"))
    {
        spdlog::error("client did not provide a content length");
        message.reply(status_codes::LengthRequired);
        return;
    }

    utility::string_t content_type;
    if (!headers.match(U("Content-Type"), content_type))
    {
        spdlog::error("could not load content type from headers");
        message.reply(status_codes::UnsupportedMediaType, U("Unsupported Media Type"));
        return;
    }

    if (content_type.rfind("image/svg+xml", 0) != 0)
    {
        spdlog::error("client provided non svg content type: {}", content_type);
        message.reply(status_codes::UnsupportedMediaType, U("Unsupported Media Type"));
        return;
    }

    int content_length = 0;
    if (!headers.match(U("Content-Length"), content_length))
    {
        spdlog::error("could not load content length from headers");
        message.reply(status_codes::LengthRequired);
        return;
    }

    if (content_length > config.max_size)
    {
        spdlog::error("request body is too big: {} > {}", content_length, config.max_size);
        message.reply(status_codes::RequestEntityTooLarge, U("Unsupported Media Type"));
        return;
    }

    // spdlog::debug("{}", message.to_string());

    auto body = message.extract_vector().get();
    if (body.size() > config.max_size)
    {
        spdlog::error("That jerk lied and the request body is too big: {} > {}", body.size(), config.max_size);
        message.reply(status_codes::RequestEntityTooLarge, U("Unsupported Media Type"));
        return;
    }

    GError *error = NULL;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    double width, height;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t status;

    auto render_start = std::chrono::steady_clock::now();

    handle = rsvg_handle_new_from_data((const guint8 *)reinterpret_cast<char *>(body.data()), (gsize)body.size(), &error);
    if (error != NULL)
    {
        spdlog::error("rsvg_handle_new_from_file error {}", error->message);
        message.reply(status_codes::InternalError, U("unable to parse payload"));
        g_error_free(error);
        return;
    }

    rsvg_handle_get_dimensions(handle, &dim);
    width = dim.width;
    height = dim.height;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    cr = cairo_create(surface);

    rsvg_handle_render_cairo(handle, cr);

    status = cairo_status(cr);
    if (status)
    {
        spdlog::error("cairo_status error {}", cairo_status_to_string(status));
        message.reply(status_codes::InternalError, U(cairo_status_to_string(status)));
        return;
    }

    std::vector<unsigned char> out;
    status = cairo_surface_write_to_png_stream(surface, cairoWriteFunc, &out);
    if (status)
    {
        spdlog::error("cairo_status error {}", cairo_status_to_string(status));
        message.reply(status_codes::InternalError, U(cairo_status_to_string(status)));
        return;
    }

    auto render_stop = std::chrono::steady_clock::now();
    auto render_duration = std::chrono::duration_cast<std::chrono::microseconds>(render_stop - render_start);
    render_latencies_.Observe(render_duration.count());

    web::http::http_response response(status_codes::OK);
    response.headers().set_content_type(U("image/png"));
    response.set_body(out);
    message.reply(response);

    server_bytes_transferred_.Increment(out.size());

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);

    auto request_stop = std::chrono::steady_clock::now();
    auto request_stop_duration = std::chrono::duration_cast<std::chrono::microseconds>(request_stop - request_start);
    server_post_latencies_.Observe(request_stop_duration.count());
};

std::vector<MetricFamily> Server::CollectMetrics() const
{
    auto collected_metrics = std::vector<MetricFamily>{};

    for (auto &&wcollectable : collectables)
    {
        auto collectable = wcollectable.lock();
        if (!collectable)
        {
            continue;
        }

        auto &&metrics = collectable->Collect();
        collected_metrics.insert(collected_metrics.end(),
                                 std::make_move_iterator(metrics.begin()),
                                 std::make_move_iterator(metrics.end()));
    }

    return collected_metrics;
}