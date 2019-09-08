# SVGer

<p align="center">
    <a href="https://www.sslhound.com/v/www.sslhound.com/" alt="www.sslhound.com status">
        <img src="https://www.sslhound.com/v/www.sslhound.com/badge.png" />
    </a>
</p>

SVGer is a lightweight SVG to PNG rendering daemon.

# Usage

From the `--help` option:

    docker run --rm -i your_favorite_tag /app/svger --help
    svger
    Usage: /app/svger [OPTIONS]
    
    Options:
      -h,--help                   Print this help message and exit
      --listen TEXT=0.0.0.0 (Env:LISTEN)
                                  Listen on a specific interface and port.
      --port INT=5003 (Env:PORT)  Listen on a specific port.
      --environment TEXT=development (Env:ENVIRONMENT)
                                  The environment.
      --backend TEXT (Env:BACKEND)
                                  The backend to render from
      --backend-insecure BOOLEAN=0 (Env:BACKEND_INSECURE)
                                  Ignore backend SSL validation.
      --max-size INT=1048576 (Env:MAX_SIZE)
                                  The max SVG size to process.
      --enable-get BOOLEAN=1 (Env:ENABLE_GET)
                                  Enable to GET method.
      --enable-post BOOLEAN=1 (Env:ENABLE_POST)
                                  Enable the POST method.

The `listen` option defaults to "0.0.0.0" and can be set with the `LISTEN` environment variable.

The `port` option defaults to "5003" and can be set with the `PORT` environment variable.

The `environment` option defaults to "development" and can be set with the `ENVIRONMENT` environment variable. Setting this to "production" will disable the "DEBUG" log level.

The `max-size` option defaults to 1048576 (1 megabyte) and can be set with the `MAX_SIZE` environment variable. Setting this to less than 1 will prevent the daemon from starting.

The `backend-insecure` option defaults to false and can be set with `BACKEND_INSECURE`. It is used to ignore validation of backend SSL certificates.

The `enable-get` option defaults to true and can be set with `ENABLE_GET`. When it is set, the `GET /`, `GET /metrics`, and `GET /*` HTTP resources are available.

The `enable-post` option defaults to true and can be set with `ENABLE_POST`. When it is set, the `POST /` HTTP resource is available.

# API

## GET /

Returns the content "OK". Can be used for health checks.

## GET /metrics

Returns prometheus metrics using a text serializer. All metrics are tagged with application=svger.

Example:

```text
# HELP server_transferred_bytes_total Transferred bytes to from svger
# TYPE server_transferred_bytes_total counter
server_transferred_bytes_total{application="svger"} 5393.000000
# HELP server_get_latencies Latencies of serving svger GET requests, in microseconds
# TYPE server_get_latencies summary
server_get_latencies_count{application="svger"} 2
server_get_latencies_sum{application="svger"} 21514991.000000
server_get_latencies{application="svger",quantile="0.500000"} 250175.000000
server_get_latencies{application="svger",quantile="0.900000"} 250175.000000
server_get_latencies{application="svger",quantile="0.990000"} 250175.000000
# HELP server_post_latencies Latencies of serving svger POST requests, in microseconds
# TYPE server_post_latencies summary
server_post_latencies_count{application="svger"} 1
server_post_latencies_sum{application="svger"} 28116.000000
server_post_latencies{application="svger",quantile="0.500000"} 28116.000000
server_post_latencies{application="svger",quantile="0.900000"} 28116.000000
server_post_latencies{application="svger",quantile="0.990000"} 28116.000000
# HELP server_get_total Number of times a GET request was made
# TYPE server_get_total counter
server_get_total{application="svger"} 5.000000
# HELP backend_post_total Number of times a POST request was made
# TYPE backend_post_total counter
backend_post_total{application="svger"} 1.000000
# HELP backend_bytes_transferred_total Transferred bytes to from backend
# TYPE backend_bytes_transferred_total counter
backend_bytes_transferred_total{application="svger"} 0.000000
# HELP backend_post_total Number of times a backend request was made
# TYPE backend_post_total counter
backend_post_total{application="svger"} 3.000000
# HELP backend_request_latencies Latencies of backend requests, in microseconds
# TYPE backend_request_latencies summary
backend_request_latencies_count{application="svger"} 3
backend_request_latencies_sum{application="svger"} 21599143.000000
backend_request_latencies{application="svger",quantile="0.500000"} 189646.000000
backend_request_latencies{application="svger",quantile="0.900000"} 201076.000000
backend_request_latencies{application="svger",quantile="0.990000"} 201076.000000
# HELP render_latencies Latencies of render work, in microseconds
# TYPE render_latencies summary
render_latencies_count{application="svger"} 3
render_latencies_sum{application="svger"} 30878.000000
render_latencies{application="svger",quantile="0.500000"} 1457.000000
render_latencies{application="svger",quantile="0.900000"} 1593.000000
render_latencies{application="svger",quantile="0.990000"} 1593.000000
```

## GET /*

Used to do read-through PNG renders of SVG content. When a request is made and the backend option set, the server will make an HTTP GET call to the backend with the URI provided in the original GET request. The response body must be SVG data. On success, PNG is returned by this endpoint.

The following headers are forwarded:

* If-Modified-Since
* If-Unmodified-Since
* If-None-Match

If the backend supplies them, the following headers are returned:

* Cache-Control
* Last-Modified
* Date

If the backend returns a 304, the server will return an empty 304.

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

    $ docker run --rm -i -p 34568:5003 your_favorite_tag /app/svger --environment production
    [2019-09-06 13:54:19.553] [info] svger starting up environment=production listen=0.0.0.0:5003
    [2019-09-06 13:54:19.557] [info] Started server on http://0.0.0.0:5003/
    Press ENTER to exit.

# Deployment

Deploying this to kubernetes is easy.

```yaml
---
apiVersion: v1
kind: Service
metadata:
  labels:
    run: svger
  name: svger
spec:
  ports:
    - port: 5004
      protocol: TCP
      targetPort: 5003
  selector:
    app: svger
    environment: production
  sessionAffinity: None
  type: ClusterIP
---
apiVersion: extensions/v1beta1
kind: Deployment
metadata:
  name: svger
  labels:
    environment: production
spec:
  replicas: 1
  template:
    metadata:
      labels:
        app: svger
        environment: production
    spec:
      imagePullSecrets:
        - name: github-registry
      containers:
        - name: svger
          image: docker.pkg.github.com/sslhound/svger/svger:v1.0.1
          imagePullPolicy: Always
          ports:
            - containerPort: 5003
          env:
            - name: ENVIRONMENT
              value: "production"
```

In your other deployments you can set the SVGer endpoint to "http://svger.default.svc.cluster.local:5004/".

# Credits

Thanks to the developers of prometheus-cpp, cpprestsdk, spdlog, and CLI11.
