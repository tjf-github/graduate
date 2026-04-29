# Cloudisk 文档导航

`docs/` 经过整理后只保留“可长期维护”的核心文档，避免内容重复和状态不一致。

## 推荐阅读顺序

1. [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md)
2. [LARGE_FILE_UPLOAD_GUIDE.md](LARGE_FILE_UPLOAD_GUIDE.md)
3. [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)
4. [ROADMAP.md](ROADMAP.md)

## 文档说明

- [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md)
  项目定位、架构、请求链路、数据库模型、接口总览、当前能力与限制。

- [LARGE_FILE_UPLOAD_GUIDE.md](LARGE_FILE_UPLOAD_GUIDE.md)
  已落地的大文件分块上传与流式下载实现说明，包含接口协议、数据表、联调和验收清单。

- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)
  单机与云服务器部署步骤、`systemd`、Nginx、验证流程。

- [ROADMAP.md](ROADMAP.md)
  后续迭代计划（含后台管理能力规划）。

## 维护规则

- 新需求优先补充到现有文档，不再新增平行“计划书”。
- 当实现状态变化时，优先同步 `PROJECT_OVERVIEW.md` 与对应专题文档。
