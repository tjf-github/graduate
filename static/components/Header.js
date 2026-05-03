import { escapeHtml } from "../utils.js";

export function renderHeader(state) {
    const breadcrumbs = state.breadcrumbs.length > 0 ? state.breadcrumbs : ["我的文件"];
    const isFileSection = ["files", "recent", "shared"].includes(state.activeSection);

    return `
        <header class="topbar">
            <div class="topbar-main">
                <div class="breadcrumbs">
                    ${breadcrumbs.map((item, index) => `
                        <span class="breadcrumb-item ${index === breadcrumbs.length - 1 ? "is-current" : ""}">${item}</span>
                    `).join('<span class="breadcrumb-sep">/</span>')}
                </div>
                <div class="topbar-title-row">
                    <div>
                        <h1>${state.sectionTitle}</h1>
                        <p>${state.sectionDescription}</p>
                    </div>
                    ${isFileSection ? renderSearchBox(state) : ""}
                </div>
            </div>

            ${renderTopbarActions(state)}
        </header>
    `;
}

function renderSearchBox(state) {
    return `
        <div class="topbar-search">
            <label class="search-box">
                <span>Search</span>
                <input
                    id="search-input"
                    type="text"
                    placeholder="搜索文件名、后缀或时间"
                    value="${escapeHtml(state.searchQuery)}"
                    autocomplete="off"
                >
            </label>
            ${state.searchSuggestions.length > 0 ? `
                <div class="search-suggestions">
                    ${state.searchSuggestions.map((keyword) => `
                        <button data-action="apply-suggestion" data-keyword="${escapeHtml(keyword)}">${keyword}</button>
                    `).join("")}
                </div>
            ` : ""}
        </div>
    `;
}

function renderTopbarActions(state) {
    const isFileSection = ["files", "recent", "shared"].includes(state.activeSection);

    if (!isFileSection) {
        return "";
    }

    return `
        <div class="topbar-actions">
            <div class="action-group">
                <button class="primary-button" data-action="pick-file">上传文件</button>
                <button class="ghost-button" data-action="refresh-files">刷新列表</button>
                <button class="ghost-button" data-action="toggle-dropzone">拖拽上传</button>
            </div>
            <div class="action-group view-switch">
                <button class="${state.viewMode === "list" ? "is-active" : ""}" data-action="set-view" data-view="list">列表</button>
                <button class="${state.viewMode === "grid" ? "is-active" : ""}" data-action="set-view" data-view="grid">网格</button>
            </div>
        </div>

        <div class="link-tools">
            <input
                id="share-link-input"
                type="text"
                placeholder="粘贴分享链接，例如 /api/share/download?code=XXXX"
            >
            <button class="ghost-button" data-action="download-share-link">下载分享文件</button>
        </div>
    `;
}
