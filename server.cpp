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

//
// A POST of the dealer resource creates a new table and returns a resource for
// that table.
//
void Server::handle_post(http_request message)
{
    message.reply(status_codes::MethodNotAllowed, U("Method not allowed"));
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
