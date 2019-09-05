FROM ubuntu as cpp-build-base

RUN apt-get update && apt-get install -y build-essential git cmake autoconf libtool pkg-config

FROM cpp-build-base AS build

RUN apt-get install -y librsvg2-dev libcairo2 libcairo2-dev

WORKDIR /src

COPY CMakeLists.txt main.cpp rsvg.c badge.svg ./
COPY modules/* ./modules/

RUN find ./

RUN cmake . && make

FROM ubuntu

RUN apt-get update && apt-get install -y libcairo2 
RUN apt-get install -y librsvg2-bin

WORKDIR /app

COPY --from=build /src/svger ./
COPY --from=build /src/badge.svg ./

CMD ["/app/svger"]
