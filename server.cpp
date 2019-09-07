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

using namespace std;
using namespace std::chrono;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;
using namespace web::http;
using namespace web::http::client;

static _cairo_status cairoWriteFunc(void *context, const unsigned char *data, unsigned int length)
{
    auto outData = static_cast<std::vector<uint8_t> *>(context);
    auto offset = static_cast<unsigned int>(outData->size());
    outData->resize(offset + length);
    memcpy(outData->data() + offset, data, length);
    return CAIRO_STATUS_SUCCESS;
}

Server::Server(ServerConfig config) : m_listener(config.addr), config(config)
{
    if (config.enable_get)
    {
        m_listener.support(methods::GET, std::bind(&Server::handle_get, this, std::placeholders::_1));
    }
    if (config.enable_post)
    {
        m_listener.support(methods::POST, std::bind(&Server::handle_post, this, std::placeholders::_1));
    }
}

void Server::handle_get(http_request message)
{
    auto start = high_resolution_clock::now();
    auto path = message.relative_uri().path();
    spdlog::debug("{}", message.to_string());

    if (path == "/")
    {
        message.reply(status_codes::OK, U("OK"));
        return;
    }

    spdlog::info("Preparing to connect to {}", config.backend + path);

    http_client_config proxy_client_config;
    proxy_client_config.set_validate_certificates(!config.backend_insecure);

    http_request proxy_request(methods::GET);

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

    http_client proxy_client(config.backend + path, proxy_client_config);

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
    response.headers().add(U("Content-Type"), match_header);

    auto body = proxy_response.extract_vector().get();
    spdlog::debug("proxy_response body {}", body.data());

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

    response.headers().set_content_type(U("image/png"));
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
