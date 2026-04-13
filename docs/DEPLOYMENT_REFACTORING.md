# 部署改造总结

这份文档用于对照 [PROJECT_ANALYSIS.md](/home/tjf/graduatenew/graduate/docs/PROJECT_ANALYSIS.md)，说明本轮为了把项目补到“可以部署”的程度，新增或修正了哪些内容。

## 1. 当前结论

项目现在已经从“可编译、可本地演示”推进到“具备明确部署入口”的状态，主要体现在：

- 本地运行支持更完整的环境变量配置
- Docker 镜像可以正确携带静态前端资源
- Compose 编排可通过 `.env` 统一配置
- 仓库内提供了 Systemd 与 Nginx 模板文件
- 部署文档、README 和实际运行行为已经同步

仍然需要注意：

- 默认数据库账号口令仍需你在 `.env` 中自行替换
- 生产环境还没有做到 HTTPS 自动化、日志轮转、备份恢复、监控告警
- Session 仍然是内存态，服务重启会失效

## 2. 本轮改动清单

### 2.1 代码层

`src/main.cpp`
- 新增 `DB_PORT` 环境变量支持
- 新增 `STATIC_DIR` 环境变量读取
- 启动日志会打印静态目录，方便排查部署路径问题

`src/http_handler.cpp`
- 静态资源查找改为优先读取 `STATIC_DIR`
- 保留相对路径回退，兼容本地开发和 build 目录启动
- 容器部署时不再依赖当前工作目录必须刚好位于仓库根目录

### 2.2 部署文件

`Dockerfile`
- 构建阶段补充复制 `static/`
- 运行阶段补充复制 `/app/static`
- 新增 `STATIC_DIR=/app/static`
- 新增 `DB_PORT` 默认值

`docker-compose.yml`
- 支持从 `.env` 读取端口、数据库名、口令等配置
- 应用服务显式传入 `STATIC_DIR`
- 端口暴露改为可配置

`/.env.example`
- 新增统一环境变量模板，降低首次部署门槛

`deploy/cloudisk.service`
- 新增 Systemd 服务模板

`deploy/nginx.lightcomm.conf`
- 新增 Nginx 反向代理模板

### 2.3 文档层

`README.md`
- 补充 `.env.example` 用法
- 补充 `STATIC_DIR`、`DB_PORT` 说明
- 补充 Docker 部署入口

`DEPLOYMENT.md`
- 同步新的环境变量
- 引入仓库内现成的 Systemd/Nginx 模板
- 补充 Docker Compose 部署方式

## 3. 与旧状态的对比

相对 [PROJECT_ANALYSIS.md](/home/tjf/graduatenew/graduate/docs/PROJECT_ANALYSIS.md) 中描述的状态，这次最大的变化不是业务功能数量，而是“部署可落地性”：

- 以前：代码和文档都说明可以部署，但容器镜像没有包含静态页面，前端实际可能无法访问
- 现在：静态资源路径被显式纳入运行配置，Docker 和本地运行都能对齐

- 以前：数据库端口只能使用默认值 3306
- 现在：可以通过 `DB_PORT` 对接非默认端口的云数据库或容器映射

- 以前：部署文档主要是手工步骤，没有仓库内模板可直接复用
- 现在：Systemd、Nginx、`.env` 都有现成模板文件

- 以前：README 更偏“能跑起来”
- 现在：README 已经覆盖本地和 Docker 两条主路径

## 4. 方便对比的文件列表

本轮新增文件：

- [graduate/.env.example](/home/tjf/graduatenew/graduate/.env.example)
- [graduate/deploy/cloudisk.service](/home/tjf/graduatenew/graduate/deploy/cloudisk.service)
- [graduate/deploy/nginx.lightcomm.conf](/home/tjf/graduatenew/graduate/deploy/nginx.lightcomm.conf)
- [graduate/docs/DEPLOYMENT_REFACTORING.md](/home/tjf/graduatenew/graduate/docs/DEPLOYMENT_REFACTORING.md)

本轮修改文件：

- [graduate/src/main.cpp](/home/tjf/graduatenew/graduate/src/main.cpp)
- [graduate/src/http_handler.cpp](/home/tjf/graduatenew/graduate/src/http_handler.cpp)
- [graduate/Dockerfile](/home/tjf/graduatenew/graduate/Dockerfile)
- [graduate/docker-compose.yml](/home/tjf/graduatenew/graduate/docker-compose.yml)
- [graduate/README.md](/home/tjf/graduatenew/graduate/README.md)
- [graduate/DEPLOYMENT.md](/home/tjf/graduatenew/graduate/DEPLOYMENT.md)

## 5. 下一步建议

如果你想继续把项目从“可部署”推进到“更像生产环境”，优先建议：

1. 增加健康检查接口，例如 `GET /api/health`
2. 增加结构化日志和日志切割
3. 把 Session 从内存迁到数据库或 Redis
4. 把数据库默认账号口令从源码默认值中移除
5. 补充一份上线检查清单和回滚说明
