# Cloudisk 文档导航

`docs/` 目录现在作为项目的唯一正式文档入口，覆盖项目背景、功能现状、大文件上传扩充和部署说明。

## 推荐阅读顺序

1. [CLOUDISK_PROJECT_SUMMARY.md](CLOUDISK_PROJECT_SUMMARY.md)
2. [PROJECT_ANALYSIS.md](PROJECT_ANALYSIS.md)
3. [LARGE_FILE_UPLOAD_PLAN.md](LARGE_FILE_UPLOAD_PLAN.md)
4. [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)

## 文档分工

- [CLOUDISK_PROJECT_SUMMARY.md](CLOUDISK_PROJECT_SUMMARY.md)
  项目定位、模块概览、链路、现状与后续方向的一页式总结。

- [PROJECT_ANALYSIS.md](PROJECT_ANALYSIS.md)
  面向接手者和答辩场景的架构分析文档，解释服务如何启动、请求如何流转、模块如何分工，也保留当前能力与扩充方向的分析入口。

- [LARGE_FILE_UPLOAD_PLAN.md](LARGE_FILE_UPLOAD_PLAN.md)
  本次新增能力的完整设计说明，覆盖数据库、后端接口、前端配合、实现约束、API 摘要和集成检查项。

- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)
  云服务器部署和环境准备指南，同时包含部署摘要和 `init.sh` 菜单说明。

## 维护约定

- 正式说明文档只维护在 `docs/` 下。
- 根目录历史 Markdown 保留为轻量入口，避免外部链接失效。
- 当前推荐长期保留的正式文档只有：
  [CLOUDISK_PROJECT_SUMMARY.md](CLOUDISK_PROJECT_SUMMARY.md)、
  [PROJECT_ANALYSIS.md](PROJECT_ANALYSIS.md)、
  [LARGE_FILE_UPLOAD_PLAN.md](LARGE_FILE_UPLOAD_PLAN.md)、
  [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)。
