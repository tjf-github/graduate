FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libmysqlclient-dev \
    libssl-dev \
    uuid-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY CMakeLists.txt /build/
COPY include/ /build/include/
COPY src/ /build/src/
COPY static/ /build/static/

RUN cmake -S /build -B /build/build && \
    cmake --build /build/build -j"$(nproc)"

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libmysqlclient21 \
    libssl3 \
    uuid-runtime \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/build/lightweight_comm_server /app/lightweight_comm_server
COPY --from=builder /build/static /app/static

RUN mkdir -p /app/storage

ENV SERVER_PORT=8080
ENV STORAGE_PATH=/app/storage
ENV STATIC_DIR=/app/static
ENV DB_HOST=mysql
ENV DB_PORT=3306
ENV DB_USER=root
ENV DB_PASSWORD=your_password
ENV DB_NAME=cloudisk

EXPOSE 8080

CMD ["/app/lightweight_comm_server"]
