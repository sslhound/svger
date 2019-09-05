/***
 * Copyright (C) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
 *
 * =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *
 * Dealer.cpp : Contains the main logic of the black jack dealer
 *
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 ****/

#include <iostream>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <cairo.h>
#include <math.h>
#include <librsvg/rsvg.h>
#include <iostream>
#include <fstream>
#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_listener.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace web;
using namespace utility;
using namespace http;
using namespace web::http::experimental::listener;

class Server
{
public:
    Server() {}
    Server(utility::string_t url);

    pplx::task<void> open() { return m_listener.open(); }
    pplx::task<void> close() { return m_listener.close(); }

private:
    void handle_get(http_request message);
    void handle_put(http_request message);
    void handle_post(http_request message);
    void handle_delete(http_request message);

    http_listener m_listener;
};

Server::Server(utility::string_t url) : m_listener(url)
{
    m_listener.support(methods::GET, std::bind(&Server::handle_get, this, std::placeholders::_1));
    m_listener.support(methods::PUT, std::bind(&Server::handle_put, this, std::placeholders::_1));
    m_listener.support(methods::POST, std::bind(&Server::handle_post, this, std::placeholders::_1));
    m_listener.support(methods::DEL, std::bind(&Server::handle_delete, this, std::placeholders::_1));
}

//
// A GET of the dealer resource produces a list of existing tables.
//
void Server::handle_get(http_request message)
{
    message.reply(status_codes::OK, U("OK"));
};

static _cairo_status cairoWriteFunc(void *closure,
                                    const unsigned char *data,
                                    unsigned int length)
{
    // cast back the vector passed as user parameter on cairo_surface_write_to_png_stream
    // see the cairo_surface_write_to_png_stream doc for details
    auto outData = static_cast<std::vector<uint8_t> *>(closure);

    auto offset = static_cast<unsigned int>(outData->size());
    outData->resize(offset + length);

    memcpy(outData->data() + offset, data, length);

    return CAIRO_STATUS_SUCCESS;
}

//
// A POST of the dealer resource creates a new table and returns a resource for
// that table.
//
void Server::handle_post(http_request message)
{
    auto body = message.extract_string(true).get();

    GError *error = NULL;
    RsvgHandle *handle;
    RsvgDimensionData dim;
    double width, height;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_status_t status;
    streampos size;

    handle = rsvg_handle_new_from_data((const guint8 *)body.c_str(), (gsize)body.size(), &error);
    if (error != NULL)
    {
        std::cerr << "rsvg_handle_new_from_file error: " << std::endl
                  << error->message << std::endl;
        message.reply(status_codes::InternalError, U(error->message));
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
        std::cerr << "cairo_status: " << std::endl
                  << cairo_status_to_string(status) << std::endl;
        message.reply(status_codes::InternalError, U(cairo_status_to_string(status)));
        return;
    }

    // unsigned char * cairo_image_surface_get_data (cairo_surface_t *surface);
    // cairo_surface_write_to_png(surface, output_filename);
    // auto data = cairo_image_surface_get_data(surface);

    std::vector<unsigned char> out;
    status = cairo_surface_write_to_png_stream(surface, cairoWriteFunc, &out);
    if (status)
    {
        std::cerr << "cairo_status: " << std::endl
                  << cairo_status_to_string(status) << std::endl;
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

//
// A DELETE of the player resource leaves the table.
//
void Server::handle_delete(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
};

//
// A PUT to a table resource makes a card request (hit / stay).
//
void Server::handle_put(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
};
