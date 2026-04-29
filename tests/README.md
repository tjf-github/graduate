# Tests

默认监听端口来自 [src/main.cpp](/home/tjf/graduatenew/src/main.cpp:27)，默认值是 `8080`。三个测试脚本都会读取 `PORT` 环境变量；未设置时使用 `8080`，因此基准地址统一是 `http://127.0.0.1:${PORT}`。

运行前置条件：

- 先启动服务端，并确保数据库已经按 [init.sql](/home/tjf/graduatenew/init.sql:1) 初始化完成。
- 测试会自动注册新用户，用户名基于时间戳生成，避免和现有数据冲突。
- `functional_test.sh` 和 `perf_test.sh` 会在 `tests/` 下创建临时文件，并通过真实接口写入数据库与存储目录。

运行方式：

- `bash tests/functional_test.sh`
- `bash tests/perf_test.sh`
- 如需指定端口：`PORT=8080 bash tests/functional_test.sh`

并发测试编译：

- 项目主程序的编译标志来自 [CMakeLists.txt](/home/tjf/graduatenew/CMakeLists.txt:4) 与 [CMakeLists.txt](/home/tjf/graduatenew/CMakeLists.txt:48)：使用 `-Wall -Wextra -O2`，主工程标准为 `C++17`。
- `tests/concurrent_test.cpp` 是独立的 POSIX socket 压测程序，按需求使用 `C++14` 编译即可：

```bash
g++ -std=c++14 -Wall -Wextra -O2 -pthread tests/concurrent_test.cpp -o tests/concurrent_test
```

并发测试运行方式：

```bash
./tests/concurrent_test
PORT=8080 ./tests/concurrent_test
```

补充说明：

- `functional_test.sh` 会覆盖注册、登录、鉴权失败、文件接口、分享接口、消息接口和服务状态接口。
- `perf_test.sh` 会生成 `100KB`、`10MB`、`100MB` 三个测试文件，测试直传接口 `/api/file/upload` 和流式下载接口 `/api/file/download/stream?id=...`。
- `concurrent_test.cpp` 的四个并发场景分别对应源码中的登录、文件列表、直传上传、消息发送与消息拉取接口。
