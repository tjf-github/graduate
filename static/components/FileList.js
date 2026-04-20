import { renderFileCard } from "./FileCard.js";

export function renderFileList(state) {
    if (state.isLoadingFiles) {
        return `
            <section class="content-card">
                <div class="section-toolbar">
                    <div>
                        <strong>文件列表</strong>
                        <p>正在同步云端文件数据</p>
                    </div>
                </div>
                <div class="skeleton-grid">
                    ${Array.from({ length: state.viewMode === "grid" ? 8 : 6 }).map(() => `
                        <div class="skeleton-card">
                            <div class="skeleton-block skeleton-thumb"></div>
                            <div class="skeleton-block skeleton-line"></div>
                            <div class="skeleton-block skeleton-line short"></div>
                        </div>
                    `).join("")}
                </div>
            </section>
        `;
    }

    if (state.fileError) {
        return `
            <section class="content-card empty-state">
                <strong>文件加载失败</strong>
                <p>${state.fileError}</p>
                <button class="primary-button" data-action="refresh-files">重新加载</button>
            </section>
        `;
    }

    if (state.visibleFiles.length === 0) {
        return `
            <section class="content-card empty-state">
                <strong>${state.searchQuery ? "没有匹配结果" : "文件空间还是空的"}</strong>
                <p>${state.searchQuery ? "换个关键词试试，或者清空搜索后查看全部文件。" : "上传第一个文件后，这里会按照网盘风格展示列表与卡片视图。"}</p>
                <div class="empty-actions">
                    <button class="primary-button" data-action="pick-file">上传文件</button>
                    <button class="ghost-button" data-action="refresh-files">刷新列表</button>
                </div>
            </section>
        `;
    }

    return `
        <section class="content-card">
            <div class="section-toolbar">
                <div>
                    <strong>文件列表</strong>
                    <p>共 ${state.visibleFiles.length} 个文件，已选择 ${state.selectedIds.size} 个</p>
                </div>
                <div class="selection-toolbar ${state.selectedIds.size > 0 ? "is-visible" : ""}">
                    <button class="ghost-button" data-action="batch-share">批量分享</button>
                    <button class="ghost-button" data-action="batch-delete">批量删除</button>
                </div>
            </div>

            ${state.viewMode === "list"
                ? renderListTable(state)
                : `<div class="file-grid">${state.visibleFiles.map((file, index) => renderFileCard(file, { index, selectedIds: state.selectedIds, sharedMap: state.sharedMap })).join("")}</div>`
            }
        </section>
    `;
}

function renderListTable(state) {
    return `
        <div class="file-table-wrap">
            <table class="file-table">
                <thead>
                    <tr>
                        <th class="checkbox-col">
                            <input type="checkbox" data-action="toggle-select-all" ${state.selectedIds.size > 0 && state.selectedIds.size === state.visibleFiles.length ? "checked" : ""}>
                        </th>
                        <th>名称</th>
                        <th>类型</th>
                        <th>大小</th>
                        <th>更新时间</th>
                        <th>状态</th>
                        <th class="action-col">操作</th>
                    </tr>
                </thead>
                <tbody>
                    ${state.visibleFiles.map((file, index) => {
                        const isSelected = state.selectedIds.has(file.id);
                        return `
                            <tr
                                class="file-row ${isSelected ? "is-selected" : ""}"
                                data-file-id="${file.id}"
                                data-index="${index}"
                            >
                                <td class="checkbox-col">
                                    <input
                                        type="checkbox"
                                        data-action="toggle-select"
                                        data-file-id="${file.id}"
                                        data-index="${index}"
                                        ${isSelected ? "checked" : ""}
                                    >
                                </td>
                                <td>
                                    <div class="file-name-cell">
                                        <span class="file-type-pill">${file.extensionLabel}</span>
                                        <div>
                                            <strong title="${escapeHtml(file.name)}">${file.name}</strong>
                                            <small>ID ${file.id}</small>
                                        </div>
                                    </div>
                                </td>
                                <td>${file.typeLabel}</td>
                                <td>${file.sizeLabel}</td>
                                <td>${file.dateLabel}</td>
                                <td>${state.sharedMap[file.id] ? '<span class="meta-chip">已分享</span>' : '<span class="meta-chip subtle">私有</span>'}</td>
                                <td class="action-col">
                                    <div class="file-inline-actions">
                                        <button class="icon-button" data-action="download-file" data-file-id="${file.id}">下载</button>
                                        <button class="icon-button" data-action="open-rename" data-file-id="${file.id}">重命名</button>
                                        <button class="icon-button" data-action="share-file" data-file-id="${file.id}">分享</button>
                                        <button class="icon-button danger" data-action="delete-file" data-file-id="${file.id}">删除</button>
                                    </div>
                                </td>
                            </tr>
                        `;
                    }).join("")}
                </tbody>
            </table>
        </div>
    `;
}

function escapeHtml(value) {
    return String(value || "")
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}
