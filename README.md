# SVGer

SVGer is a lightweight SVG to PNG rendering daemon.

#

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
    @badge.svg
    EOF
    $ cat load | vegeta attack -duration=60s | vegeta report
    Requests      [total, rate, throughput]  3000, 50.02, 50.01
    Duration      [total, attack, wait]      59.9831289s, 59.981168s, 1.9609ms
    Latencies     [mean, 50, 95, 99, max]    1.775476ms, 1.974156ms, 2.024575ms, 2.05819ms, 7.0521ms
    Bytes In      [total, mean]              8115000, 2705.00
    Bytes Out     [total, mean]              2766000, 922.00
    Success       [ratio]                    100.00%
    Status Codes  [code:count]               200:3000
    Error Set:

# Build and run

docker build -t svger:dev --progress=plain .
docker run -v d:/Projects/sslhound/svger/results:/app/results/ svger:dev