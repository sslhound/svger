# SVGer

SVGer is a lightweight SVG to PNG rendering daemon.

# Usage

From the `--help` option:

    docker run --rm -i your_favorite_tag /app/svger --help
    svger
    Usage: /app/svger [OPTIONS]
    
    Options:
      -h,--help                   Print this help message and exit
      --listen TEXT=http://0.0.0.0:5003 (Env:LISTEN)
                                  Listen on a specific interface and port.
      --environment TEXT=development (Env:ENVIRONMENT)
                                  The environment.
      --max-size INT=1048576 (Env:MAX_SIZE)
                                  The max SVG size to process.

The `listen` option defaults to "0.0.0.0:5003" and can be set with the `LISTEN` environment variable.

The `environment` option defaults to "development" and can be set with the `ENVIRONMENT` environment variable. Setting this to "production" will disable the "DEBUG" log level.

The `max-size` option defaults to 1048576 (1 megabyte) and can be set with the `MAX_SIZE` environment variable. Setting this to less than 1 will prevent the daemon from starting.

# API

## POST /

Submit an HTTP POST request to "/".

* The request must have the "Content-Type" header set to "image/svg+xml".
* The request body must be the SVG XML to be rendered.

On success, the response will be a HTTP 200 OK with the "Content-Type" header set to "image/png" and the response body being the image data.

On failure, the response may be ...

* HTTP 415 when the content type is not "image/svg+xml"
* HTTP 413 when the request body is too big
* HTTP 500 when there is an internal error converting the SVG to a PNG

# Performance Results

    $ cat > badge.svg<<EOF
    <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="192" height="20">
    <linearGradient id="smooth" x2="0" y2="100%">
    <stop offset="0" stop-color="#bbb" stop-opacity=".1"/>
    <stop offset="1" stop-opacity=".1"/>
    </linearGradient>
    <mask id="round">
    <rect width="192" height="20" rx="3" fill="#fff"/>
    </mask>
    <g mask="url(#round)">
    <rect width="74" height="20" fill="#555"/>
    <rect x="74" width="118" height="20" fill="#97ca00"/>
    <rect width="192" height="20" fill="url(#smooth)"/>
    </g>
    <g fill="#fff" text-anchor="middle" font-family="DejaVu Sans,Verdana,Geneva,sans-serif" font-size="11">
    <text x="38" y="15" fill="#010101" fill-opacity=".3">SSL Hound</text>
    <text x="38" y="14">SSL Hound</text>
    <text x="132" y="15" fill="#010101" fill-opacity=".3">sslhound.com:443</text>
    <text x="132" y="14">sslhound.com:443</text>
    </g>
    </svg>
    EOF
    $ cat > load<<EOF
    POST http://localhost:34568/
    Content-Type: image/svg+xml
    @badge.svg
    EOF
    $ cat load | vegeta attack -duration=300s -rate 300 | vegeta report
    Requests      [total, rate, throughput]  90000, 300.00, 300.00
    Duration      [total, attack, wait]      4m59.9979737s, 4m59.9959831s, 1.9906ms
    Latencies     [mean, 50, 95, 99, max]    1.864406ms, 1.976984ms, 2.026712ms, 2.115462ms, 19.0259ms
    Bytes In      [total, mean]              243450000, 2705.00
    Bytes Out     [total, mean]              82980000, 922.00
    Success       [ratio]                    100.00%
    Status Codes  [code:count]               200:90000
    Error Set:

# Build and run

Build the container.

    $ docker build -t your_favorite_tag .

Run the container.

    $ docker run --rm -i -p 34568:5003 your_favorite_tag /app/svger --listen 0.0.0.0:5003 --environment production
    [2019-09-06 13:54:19.553] [info] svger starting up environment=production listen=0.0.0.0:5003
    [2019-09-06 13:54:19.557] [info] Started server on http://0.0.0.0:5003/
    Press ENTER to exit.
