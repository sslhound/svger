#define _USE_MATH_DEFINES

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
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

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

std::unique_ptr<Server> g_httpServer;

void on_initialize(const string_t &address)
{
    uri_builder uri(address);
    uri.append_path(U("/"));

    utility::string_t addr = uri.to_uri().to_string();
    g_httpServer = std::unique_ptr<Server>(new Server(addr));
    g_httpServer->open().wait();

    ucout << utility::string_t(U("Listening for requests at: ")) << addr << std::endl;

    return;
}

void on_shutdown()
{
    g_httpServer->close().wait();
    return;
}

int main(int argc, char *argv[])
{
    std::cout << "Hello, World!" << std::endl;

    // printf("Hello!\n");

    // GError *error = NULL;
    // RsvgHandle *handle;
    // RsvgDimensionData dim;
    // double width, height;
    // const char *filename = "badge.svg";
    // const char *output_filename = "results/badge.png";
    // cairo_surface_t *surface;
    // cairo_t *cr;
    // cairo_status_t status;
    // int mode = 1;
    // char *memblock;
    // streampos size;

    // //g_type_init ();

    // // RsvgHandle *rsvg_handle_new_from_data (const guint8 * data, gsize data_len, GError ** error);
    // // read svg file
    // ifstream file(filename, ios::in | ios::binary | ios::ate);
    // std::cout << "Try to read file!" << std::endl;
    // if (file.is_open())
    // {
    //     size = file.tellg();
    //     memblock = new char[size];
    //     file.seekg(0, ios::beg);
    //     file.read(memblock, size);
    //     file.close();
    //     printf("SVG File Content!\n");
    //     printf("%s\n", memblock);
    // }

    // rsvg_set_default_dpi(72.0);
    // handle = rsvg_handle_new_from_data((const guint8 *)memblock, (gsize)size, &error);
    // //handle = rsvg_handle_new_from_file (filename, &error);
    // if (error != NULL)
    // {
    //     printf("rsvg_handle_new_from_file error!\n");
    //     printf("%s\n", error->message);
    //     return 1;
    // }

    // rsvg_handle_get_dimensions(handle, &dim);
    // width = dim.width;
    // height = dim.height;

    // surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    // cr = cairo_create(surface);

    // rsvg_handle_render_cairo(handle, cr);

    // status = cairo_status(cr);
    // if (status)
    // {
    //     printf("cairo_status!\n");
    //     printf("%s\n", cairo_status_to_string(status));
    //     return 1;
    // }

    // // unsigned char * cairo_image_surface_get_data (cairo_surface_t *surface);
    // cairo_surface_write_to_png(surface, output_filename);

    // int raw_width = cairo_image_surface_get_width(surface);
    // int raw_height = cairo_image_surface_get_height(surface);
    // int row_byte_size = cairo_image_surface_get_stride(surface);
    // printf("\nWIDTH: %d, HEIGHT: %d, row_bytes=%d\n", raw_width, raw_height, row_byte_size);

    // unsigned char *raw_buffer = cairo_image_surface_get_data(surface);

    // FILE *pFile;
    // pFile = fopen("results/myfile.bin", "wb");
    // fwrite(raw_buffer, sizeof(char), raw_height * row_byte_size, pFile);
    // fclose(pFile);

    // cairo_destroy(cr);
    // cairo_surface_destroy(surface);

    // delete[] memblock;

    utility::string_t port = U("34568");
    if (argc == 2)
    {
        port = argv[1];
    }

    utility::string_t address = U("http://0.0.0.0:");
    address.append(port);

    on_initialize(address);
    std::cout << "Press ENTER to exit." << std::endl;

    std::string line;
    std::getline(std::cin, line);

    on_shutdown();
    return 0;
}