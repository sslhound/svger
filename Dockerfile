# syntax=docker/dockerfile:experimental
FROM ubuntu as cpp-build-base
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential git cmake autoconf libtool pkg-config
RUN apt-get install -y librsvg2-dev libcairo2 libcairo2-dev libcpprest-dev cppcheck
RUN apt-get install -y libcurl4-openssl-dev

WORKDIR /src/third
RUN git clone https://github.com/jupp0r/prometheus-cpp.git
RUN cd prometheus-cpp && git submodule init && git submodule update && cmake . && make install

FROM cpp-build-base AS build
WORKDIR /src
COPY CMakeLists.txt main.cpp server.hpp server.cpp ./
COPY include ./include
RUN cmake . && make

FROM ubuntu
RUN apt-get update && apt-get install -y libcairo2 librsvg2-bin libcpprest
WORKDIR /app
COPY --from=build /src/svger ./
CMD ["/app/svger"]