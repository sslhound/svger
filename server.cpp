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

using namespace std;
using namespace std::chrono;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;
using namespace web::http;
using namespace web::http::client;

class Server
{
public:
    Server() {}
    Server(utility::string_t url, int max_size, utility::string_t backend, bool backend_insecure);

    pplx::task<void> open() { return m_listener.open(); }
    pplx::task<void> close() { return m_listener.close(); }

private:
    void handle_get(http_request message);
    void handle_put(http_request message);
    void handle_post(http_request message);
    void handle_delete(http_request message);

    http_listener m_listener;
    int max_size;
    utility::string_t backend;
    bool backend_insecure;
};

static _cairo_status cairoWriteFunc(void *context, const unsigned char *data, unsigned int length)
{
    auto outData = static_cast<std::vector<uint8_t> *>(context);
    auto offset = static_cast<unsigned int>(outData->size());
    outData->resize(offset + length);
    memcpy(outData->data() + offset, data, length);
    return CAIRO_STATUS_SUCCESS;
}

Server::Server(utility::string_t url, int max_size, utility::string_t backend, bool backend_insecure) : m_listener(url), max_size(max_size), backend(backend), backend_insecure(backend_insecure)
{
    m_listener.support(methods::GET, std::bind(&Server::handle_get, this, std::placeholders::_1));
    m_listener.support(methods::PUT, std::bind(&Server::handle_put, this, std::placeholders::_1));
    m_listener.support(methods::POST, std::bind(&Server::handle_post, this, std::placeholders::_1));
    m_listener.support(methods::DEL, std::bind(&Server::handle_delete, this, std::placeholders::_1));
}

void Server::handle_get(http_request message)
{
    auto start = high_resolution_clock::now();
    auto path = message.relative_uri().path();
    spdlog::info("GET request for {}", path);
    spdlog::info("{}", message.to_string());

    if (path == "/")
    {
        message.reply(status_codes::OK, U("OK"));
        return;
    }

    spdlog::info("Preparing to connect to {}", backend + path);

    // proxy_request.headers().add(L"Content-Type", L"application/json; charset=UTF-8");
    // proxy_request.headers().add(L"Log-Type", log_type.c_str());
    // proxy_request.headers().add(L"x-ms-date", rfcDate.c_str());
    // proxy_request.headers().add(L"Authorization", signature.c_str());

    // .then([=](http_response response) {
    //     printf("Received response status code:%u\n", response.status_code());

    //     // Write response body into the file.
    //     return response.body().read_to_end(fileStream->streambuf());
    // })

    http_client_config proxy_client_config;
    proxy_client_config.set_validate_certificates(!backend_insecure);

    http_request proxy_request(methods::GET);

    // if-modified-since, if-unmodified-since, and if-none-match
    auto headers = message.headers();
    utility::string_t match_header;
    if (headers.match(U("if-modified-since"), match_header))
    {
        proxy_request.headers().add(U("if-modified-since"), match_header);
    }
    if (headers.match(U("if-unmodified-since"), match_header))
    {
        proxy_request.headers().add(U("if-unmodified-since"), match_header);
    }
    if (headers.match(U("if-none-match"), match_header))
    {
        proxy_request.headers().add(U("if-none-match"), match_header);
    }

    http_client proxy_client(backend + path, proxy_client_config);
    spdlog::info("Created client");

    http_response proxy_response;

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

    spdlog::info("response status_code {}", proxy_response.status_code());

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

    web::http::http_response response(status_codes::OK);

    auto response_headers = proxy_response.headers();
    if (response_headers.match(U("Cache-Control"), match_header))
    {
        response.headers().add(U("Cache-Control"), match_header);
    }
    if (response_headers.match(U("Last-Modified"), match_header))
    {
        response.headers().add(U("Last-Modified"), match_header);
    }
    if (response_headers.match(U("Content-Type"), match_header))
    {
        response.headers().add(U("Content-Type"), match_header);
    }

    auto body = proxy_response.extract_vector().get();
    spdlog::info("proxy_response body {}", body.data());

    GError *error = NULL;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    double width, height;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t status;

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

    spdlog::info("preparing to write {} bytes", out.size());
    response.set_body(out);
    message.reply(response);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    spdlog::info("Completed render time={}us payload_size={} result_size={}", duration.count(), body.size(), out.size());
};

void Server::handle_post(http_request message)
{
    auto start = high_resolution_clock::now();
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

    if (content_type != "image/svg+xml")
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

    if (content_length > max_size)
    {
        spdlog::error("request body is too big: {} > {}", content_length, max_size);
        message.reply(status_codes::RequestEntityTooLarge, U("Unsupported Media Type"));
        return;
    }

    // spdlog::debug("{}", message.to_string());

    auto body = message.extract_vector().get();
    if (body.size() > max_size)
    {
        spdlog::error("That jerk lied and the request body is too big: {} > {}", body.size(), max_size);
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

    web::http::http_response response(status_codes::OK);
    response.headers().set_content_type(U("image/png"));
    response.set_body(out);
    message.reply(response);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    spdlog::info("Completed render time={}us payload_size={}", duration.count(), body.size());
};

void Server::handle_delete(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
};

void Server::handle_put(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
};
