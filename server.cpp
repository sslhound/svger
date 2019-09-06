#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fstream>
#include <algorithm>
#include <random>
#include <sstream>
#include <vector>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include <boost/thread/mutex.hpp>
#include "spdlog/spdlog.h"

using namespace std;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;

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
    boost::mutex mutex;
    int max_size;
};

static _cairo_status cairoWriteFunc(void *context, const unsigned char *data, unsigned int length)
{
    auto outData = static_cast<std::vector<uint8_t> *>(context);
    auto offset = static_cast<unsigned int>(outData->size());
    outData->resize(offset + length);
    memcpy(outData->data() + offset, data, length);
    return CAIRO_STATUS_SUCCESS;
}

Server::Server(utility::string_t url, int max_size) : m_listener(url), max_size(max_size)
{
    m_listener.support(methods::GET, std::bind(&Server::handle_get, this, std::placeholders::_1));
    m_listener.support(methods::PUT, std::bind(&Server::handle_put, this, std::placeholders::_1));
    m_listener.support(methods::POST, std::bind(&Server::handle_post, this, std::placeholders::_1));
    m_listener.support(methods::DEL, std::bind(&Server::handle_delete, this, std::placeholders::_1));
}

void Server::handle_get(http_request message)
{
    message.reply(status_codes::OK, U("OK"));
};

void Server::handle_post(http_request message)
{
    auto headers = message.headers();

    if (!headers.has("Content-Type"))
    {
        spdlog::error("client did not provide a content type");
        message.reply(status_codes::UnsupportedMediaType, U("Unsupported Media Type"));
        return;
    }

    utility::string_t content_type;
    if (headers.match(U("Content-Type"), content_type))
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

    boost::mutex::scoped_lock scoped_lock(mutex);

    auto body = message.extract_string(true).get();
    spdlog::debug("render request {}", body);

    GError *error = NULL;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    double width, height;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t status;

    handle = rsvg_handle_new_from_data((const guint8 *)body.c_str(), (gsize)body.size(), &error);
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
    response.headers().set_content_type(U("image/svg+xml"));
    response.set_body(out);
    message.reply(response);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);
};

void Server::handle_delete(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
};

void Server::handle_put(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
};
