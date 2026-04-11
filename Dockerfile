# 多阶段构建 - 编译阶段
FROM ubuntu:22.04 AS builder

# 设置非交互模式
ENV DEBIAN_FRONTEND=noninteractive

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libmysqlclient-dev \
    libssl-dev \
    uuid-dev \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /build

# 复制源代码
COPY CMakeLists.txt /build/
COPY include/ /build/include/
COPY src/ /build/src/

# 创建构建目录并编译
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# 运行阶段
FROM ubuntu:22.04

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    libmysqlclient21 \
    libssl3 \
    uuid-runtime \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
WORKDIR /app

# 从构建阶段复制可执行文件
COPY --from=builder /build/build/lightweight_comm_server /app/

# 创建存储目录
RUN mkdir -p /app/storage

# 设置环境变量
ENV SERVER_PORT=8080
ENV STORAGE_PATH=/app/storage
ENV DB_HOST=mysql
ENV DB_USER=root
ENV DB_PASSWORD=your_password
ENV DB_NAME=cloudisk

# 暴露端口
EXPOSE 8080

# 启动服务器
CMD ["/app/lightweight_comm_server"]
