FROM ubuntu as cpp-build-base
RUN apt-get update && apt-get install -y build-essential git cmake autoconf libtool pkg-config
RUN apt-get install -y librsvg2-dev libcairo2 libcairo2-dev libcpprest-dev cppcheck

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