import { renderSidebar } from "./components/Sidebar.js";
import { renderHeader } from "./components/Header.js";
import { renderFileList } from "./components/FileList.js";
import { renderContextMenu } from "./components/ContextMenu.js";
import { renderMessagePanel } from "./components/MessagePanel.js";
import { escapeHtml, formatBytes } from "./utils.js";

const API_BASE = "/api";
const CHUNK_SIZE = 4 * 1024 * 1024;
const CHUNK_CONCURRENCY = 3;
const LARGE_FILE_THRESHOLD = CHUNK_SIZE * 2;
const app = document.getElementById("app");
const hiddenFileInput = document.getElementById("hidden-file-input");

const state = {
    currentToken: localStorage.getItem("lwcs_token") || "",
    currentUser: localStorage.getItem("lwcs_user") || "",
    currentUserId: Number(localStorage.getItem("lwcs_user_id") || 0),
    activeChatUserId: Number(localStorage.getItem("lwcs_chat_user_id") || 0),
    authMode: "login",
    activeSection: "files",
    viewMode: localStorage.getItem("lwcs_view_mode") || "list",
    showShareInput: false,
    searchQuery: "",
    searchHistory: JSON.parse(localStorage.getItem("lwcs_search_history") || "[]"),
    profile: {
        id: 0,
        username: "",
        email: "",
        createdAt: "-",
        timeoutMinutes: 0,
        storageUsed: 0,
        storageLimit: 0
    },
    files: [],
    visibleFiles: [],
    sharedMap: JSON.parse(localStorage.getItem("lwcs_shared_map") || "{}"),
    selectedIds: new Set(),
    lastSelectedIndex: -1,
    isLoadingFiles: false,
    fileError: "",
    showDropzone: false,
    dragActive: false,
    dragDepth: 0,
    upload: {
        inProgress: false,
        progress: 0,
        filename: "",
        currentIndex: 0,
        total: 0,
        mode: "",
        statusText: "",
        cancelRequested: false,
        activeUploadId: "",
        abortController: null
    },
    modal: {
        type: "",
        payload: null
    },
    toastList: [],
    contextMenu: {
        visible: false,
        x: 0,
        y: 0,
        fileId: null
    },
    serverStatus: {
        summary: "等待同步",
        detail: "服务器状态会在登录后自动刷新。",
        clients: []
    },
    messages: [],
    messageStatus: "请选择一个用户开始聊天",
    messageLoading: false,
    messagePollTimer: null,
    statusPollTimer: null,
    statusLastUpdated: ""
};

window.addEventListener("load", init);
document.addEventListener("click", onDocumentClick);
document.addEventListener("input", onDocumentInput);
document.addEventListener("change", onDocumentChange);
document.addEventListener("submit", onDocumentSubmit);
document.addEventListener("contextmenu", onDocumentContextMenu);
document.addEventListener("keydown", onDocumentKeyDown);
document.addEventListener("dragenter", onDragEnter);
document.addEventListener("dragover", onDragOver);
document.addEventListener("dragleave", onDragLeave);
document.addEventListener("drop", onDrop);
hiddenFileInput.addEventListener("change", handleHiddenFilePick);

function init() {
    if (state.currentToken && state.currentUser) {
        bootstrapAuthorizedApp();
    } else {
        clearAuthState();
        render();
    }
}

async function bootstrapAuthorizedApp() {
    render();
    await Promise.all([loadUserProfile(), loadFileList(), loadServerStatus()]);
    restoreChatState();
    startStatusPolling();
    render();
}

function render() {
    app.innerHTML = state.currentToken && state.currentUser
        ? renderAppShell()
        : renderAuthShell();
}

function renderAuthShell() {
    return `
        <div class="auth-shell">
            <div class="auth-card">
                <section class="auth-hero">
                    <div class="auth-hero-badge">Cloud Disk Experience</div>
                    <h1>通信云服务</h1>
                    <p>文件管理、搜索、分享与消息，整合于一套清晰的现代界面——开箱即用，也易于二次开发。</p>
                    <div class="hero-metrics">
                        <div><strong>2</strong><span>种视图模式</span></div>
                        <div><strong>Shift</strong><span>范围快速多选</span></div>
                        <div><strong>实时</strong><span>搜索过滤响应</span></div>
                    </div>
                </section>
                <section class="auth-panel">
                    <div class="auth-toggle">
                        <button class="${state.authMode === "login" ? "is-active" : ""}" data-action="set-auth-mode" data-mode="login">登录</button>
                        <button class="${state.authMode === "register" ? "is-active" : ""}" data-action="set-auth-mode" data-mode="register">注册</button>
                    </div>
                    ${state.authMode === "login" ? renderLoginForm() : renderRegisterForm()}
                </section>
            </div>
            ${renderToasts()}
        </div>
    `;
}

function renderLoginForm() {
    return `
        <form class="auth-form" data-form="login">
            <h2>欢迎回来</h2>
            <p>登录后进入 LightComm Cloud，继续管理文件、分享链接与消息会话。</p>
            <div class="field">
                <label for="login-username">用户名</label>
                <input id="login-username" name="username" type="text" placeholder="请输入用户名" required>
            </div>
            <div class="field">
                <label for="login-password">密码</label>
                <input id="login-password" name="password" type="password" placeholder="请输入密码" required>
            </div>
            <button class="primary-button full-width" type="submit">登录进入工作台</button>
        </form>
    `;
}

function renderRegisterForm() {
    return `
        <form class="auth-form" data-form="register">
            <h2>创建账号</h2>
            <p>注册后即可体验上传、下载、分享和即时消息功能。</p>
            <div class="field">
                <label for="reg-username">用户名</label>
                <input id="reg-username" name="username" type="text" placeholder="请设置用户名" required>
            </div>
            <div class="field">
                <label for="reg-email">邮箱</label>
                <input id="reg-email" name="email" type="email" placeholder="name@example.com" required>
            </div>
            <div class="field">
                <label for="reg-password">密码</label>
                <input id="reg-password" name="password" type="password" placeholder="请设置密码" required>
            </div>
            <button class="primary-button full-width" type="submit">立即注册</button>
        </form>
    `;
}

function renderAppShell() {
    return `
        <div class="shell">
            <div class="app-shell">
                ${renderSidebar(buildRenderState())}
                <main class="main-panel">
                    ${renderHeader(buildRenderState())}
                    ${["files", "recent", "shared"].includes(state.activeSection) ? renderDashboard() : ""}
                    ${["files", "recent", "shared"].includes(state.activeSection) ? renderDropzone() : ""}
                    <div class="content-stack">
                        ${state.activeSection === "messages"
                            ? renderMessagesSection()
                            : state.activeSection === "status"
                                ? renderStatusSection()
                                : renderFileList(buildRenderState())}
                    </div>
                </main>
            </div>
            ${renderContextMenu(state)}
            ${renderModal()}
            ${renderToasts()}
        </div>
    `;
}

function renderDashboard() {
    const totalFiles = state.files.length;
    const sharedCount = Object.keys(state.sharedMap).length;

    return `
        <section class="dashboard-grid">
            <article class="metric-card">
                <span>文件总数</span>
                <strong>${totalFiles}</strong>
                <p>支持列表与网格两种展示方式</p>
            </article>
            <article class="metric-card">
                <span>已分享</span>
                <strong>${sharedCount}</strong>
                <p>创建链接后会在“我的分享”中汇总</p>
            </article>
        </section>
    `;
}

function renderDropzone() {
    const isVisible = state.showDropzone || state.dragActive;
    const hasQueue = state.upload.total > 1;
    return `
        <section class="dropzone ${isVisible ? "is-visible" : ""} ${state.dragActive ? "is-active" : ""}" id="dropzone">
            <strong>拖拽文件到这里即可上传</strong>
            <p>支持拖拽或选择文件，上传完成后会自动刷新列表。</p>
            ${state.upload.inProgress ? `
                <div class="upload-status">
                    <strong>${state.upload.filename || "正在上传"}${hasQueue ? `（${state.upload.currentIndex}/${state.upload.total}）` : ""}</strong>
                    <div class="progress-track">
                        <div class="progress-fill" style="width:${state.upload.progress}%"></div>
                    </div>
                    <p>上传进度 ${state.upload.progress}%${state.upload.statusText ? ` · ${state.upload.statusText}` : ""}</p>
                    ${state.upload.mode === "chunk" ? `
                        <div class="empty-actions">
                            <button class="ghost-button" data-action="cancel-upload">${state.upload.cancelRequested ? "正在取消..." : "取消上传"}</button>
                        </div>
                    ` : ""}
                </div>
            ` : `
                <div class="empty-actions">
                    <button class="primary-button" data-action="pick-file">选择文件</button>
                    <button class="ghost-button" data-action="toggle-dropzone">收起面板</button>
                </div>
            `}
        </section>
    `;
}

function renderAuxPanels() {
    return `
        <section class="grid-single">
            <article class="status-panel">
                <div class="section-toolbar">
                    <div>
                        <strong>服务器状态</strong>
                        <p>展示活跃连接与服务可用性</p>
                    </div>
                    <small>每 30 秒自动刷新${state.statusLastUpdated ? `　${state.statusLastUpdated}` : ""}</small>
                </div>
                <div class="server-status-list">
                    <div class="server-status-item">
                        <strong>${state.serverStatus.summary}</strong>
                        <p>${state.serverStatus.detail}</p>
                    </div>
                    ${(state.serverStatus.clients || []).map((client) => `
                        <div class="server-status-item">
                            <strong>${client.ip}</strong>
                            <p>socket fd: ${client.socket_fd}</p>
                        </div>
                    `).join("") || '<div class="server-status-item"><strong>暂无活跃连接明细</strong><p>当有新连接接入时会显示在这里。</p></div>'}
                </div>
            </article>
            ${renderMessagePanel(state, { compact: true })}
        </section>
    `;
}

function renderMessagesSection() {
    return renderMessagePanel(state, { compact: false });
}

function renderStatusSection() {
    return `
        <section class="content-card">
            <div class="section-toolbar">
                <div>
                    <strong>服务器状态</strong>
                    <p>每 30 秒自动刷新${state.statusLastUpdated ? `　最后更新：${state.statusLastUpdated}` : ""}</p>
                </div>
            </div>
            <div class="server-status-list">
                <div class="server-status-item">
                    <strong>${state.serverStatus.summary}</strong>
                    <p>${state.serverStatus.detail}</p>
                </div>
                ${(state.serverStatus.clients || []).map((client) => `
                    <div class="server-status-item">
                        <strong>${client.ip}</strong>
                        <p>socket fd: ${client.socket_fd}</p>
                    </div>
                `).join("") || '<div class="server-status-item"><strong>暂无活跃连接明细</strong><p>当有新连接接入时会显示在这里。</p></div>'}
            </div>
        </section>
    `;
}

function renderModal() {
    if (!state.modal.type) {
        return "";
    }

    if (state.modal.type === "rename") {
        return `
            <div class="modal-layer" data-action="close-modal">
                <div class="modal" data-stop-close="true">
                    <h3>重命名文件</h3>
                    <p>更新展示名称，不影响文件内容本身。</p>
                    <form data-form="rename">
                        <div class="field">
                            <label for="rename-input">新文件名</label>
                            <input id="rename-input" name="name" type="text" value="${escapeHtml(state.modal.payload.name)}" required>
                        </div>
                        <div class="modal-actions">
                            <button type="button" class="ghost-button" data-action="close-modal">取消</button>
                            <button type="submit" class="primary-button">保存</button>
                        </div>
                    </form>
                </div>
            </div>
        `;
    }

    if (state.modal.type === "profile") {
        return `
            <div class="modal-layer" data-action="close-modal">
                <div class="modal" data-stop-close="true">
                    <h3>编辑资料</h3>
                    <p>同步更新用户名和邮箱信息。</p>
                    <form data-form="profile">
                        <div class="field">
                            <label for="profile-username-input">用户名</label>
                            <input id="profile-username-input" name="username" type="text" value="${escapeHtml(state.profile.username)}" required>
                        </div>
                        <div class="field">
                            <label for="profile-email-input">邮箱</label>
                            <input id="profile-email-input" name="email" type="email" value="${escapeHtml(state.profile.email)}" required>
                        </div>
                        <div class="modal-actions">
                            <button type="button" class="ghost-button" data-action="close-modal">取消</button>
                            <button type="submit" class="primary-button">保存资料</button>
                        </div>
                    </form>
                </div>
            </div>
        `;
    }

    if (state.modal.type === "share") {
        return `
            <div class="modal-layer" data-action="close-modal">
                <div class="modal" data-stop-close="true">
                    <h3>分享链接已生成</h3>
                    <p>可以直接复制，也可以粘贴到顶部搜索旁的分享输入区继续使用。</p>
                    <div class="field">
                        <label for="share-link-output">分享链接</label>
                        <input id="share-link-output" type="text" readonly value="${escapeHtml(state.modal.payload.url)}">
                    </div>
                    <div class="modal-actions">
                        <button type="button" class="ghost-button" data-action="close-modal">关闭</button>
                        <button type="button" class="primary-button" data-action="copy-share-link">复制链接</button>
                    </div>
                </div>
            </div>
        `;
    }

    return "";
}

function renderToasts() {
    return `
        <div class="toast-stack">
            ${state.toastList.map((toast) => `
                <div class="toast ${toast.type}">${escapeHtml(toast.text)}</div>
            `).join("")}
        </div>
    `;
}

function buildRenderState() {
    const visibleFiles = computeVisibleFiles();
    state.visibleFiles = visibleFiles;

    const meta = getSectionMeta(state.activeSection);
    return {
        ...state,
        visibleFiles,
        sectionTitle: meta.title,
        sectionDescription: meta.description,
        breadcrumbs: meta.breadcrumbs,
        searchSuggestions: getSearchSuggestions(),
        formatBytes
    };
}

function getSectionMeta(section) {
    const map = {
        files: {
            title: "全部文件",
            description: "文件信息更紧凑，操作层级更清晰，接近成熟云盘的管理体验。",
            breadcrumbs: ["我的文件", "全部文件"]
        },
        recent: {
            title: "最近",
            description: "按上传时间倒序浏览，快速定位新文件。",
            breadcrumbs: ["我的文件", "最近"]
        },
        shared: {
            title: "我的分享",
            description: "集中查看当前会话中创建过的分享链接。",
            breadcrumbs: ["我的文件", "分享中心"]
        },
        messages: {
            title: "消息通信",
            description: "保留现有消息接口，用更清晰的会话布局承载。",
            breadcrumbs: ["工作台", "消息中心"]
        },
        status: {
            title: "系统状态",
            description: "查看服务器连接数、活跃客户端与自动刷新记录。",
            breadcrumbs: ["工作台", "系统状态"]
        }
    };

    return map[section] || map.files;
}

function computeVisibleFiles() {
    let files = state.files.map(normalizeFile);

    if (state.activeSection === "recent") {
        files = [...files].sort((a, b) => String(b.rawDate).localeCompare(String(a.rawDate)));
    }

    if (state.activeSection === "shared") {
        files = files.filter((file) => state.sharedMap[file.id]);
    }

    if (state.searchQuery.trim()) {
        const keyword = state.searchQuery.trim().toLowerCase();
        files = files.filter((file) => {
            return [
                file.name,
                file.typeLabel,
                file.extensionLabel,
                file.dateLabel
            ].some((field) => String(field).toLowerCase().includes(keyword));
        });
    }

    return files;
}

function normalizeFile(file) {
    const name = file.original_filename || file.filename || `file-${file.id}`;
    const extension = name.includes(".") ? name.split(".").pop().toUpperCase() : "FILE";
    return {
        id: Number(file.id),
        name,
        raw: file,
        rawDate: file.upload_date || "",
        dateLabel: file.upload_date || "-",
        sizeLabel: formatBytes(Number(file.size || 0)),
        typeLabel: inferFileType(name),
        extensionLabel: extension.slice(0, 4),
        thumbnail: inferThumbnail(name)
    };
}

function inferFileType(name) {
    const lower = name.toLowerCase();
    if (/\.(png|jpg|jpeg|gif|webp|svg)$/.test(lower)) return "图片";
    if (/\.(mp4|mov|avi|mkv|webm)$/.test(lower)) return "视频";
    if (/\.(pdf|doc|docx|ppt|pptx|xls|xlsx)$/.test(lower)) return "文档";
    if (/\.(zip|rar|7z|tar|gz)$/.test(lower)) return "压缩包";
    if (/\.(mp3|wav|aac|flac)$/.test(lower)) return "音频";
    return "文件";
}

function inferThumbnail(name) {
    const lower = name.toLowerCase();
    if (/\.(png|jpg|jpeg|gif|webp|svg)$/.test(lower)) {
        return "";
    }
    if (/\.(mp4|mov|avi|mkv|webm)$/.test(lower)) {
        return "";
    }
    return "";
}

function onDocumentClick(event) {
    const stopCloseNode = event.target.closest("[data-stop-close='true']");
    const actionNode = event.target.closest("[data-action]");
    const fileContainer = event.target.closest("[data-file-id][data-index]");

    if (state.contextMenu.visible && !event.target.closest(".context-menu")) {
        hideContextMenu();
    }

    if (!actionNode) {
        if (
            fileContainer &&
            !event.target.closest("button") &&
            !event.target.closest("input") &&
            !event.target.closest("label")
        ) {
            toggleFileSelection(Number(fileContainer.dataset.fileId), Number(fileContainer.dataset.index), event);
            return;
        }
        if (!stopCloseNode && event.target.classList.contains("modal-layer")) {
            closeModal();
        }
        return;
    }

    const { action } = actionNode.dataset;

    if (action === "set-auth-mode") {
        state.authMode = actionNode.dataset.mode;
        render();
        return;
    }

    if (action === "switch-section") {
        state.activeSection = actionNode.dataset.section;
        state.selectedIds = new Set();
        hideContextMenu();
        if (state.activeSection === "messages") {
            startMessagePolling();
        }
        render();
        return;
    }

    if (action === "set-view") {
        state.viewMode = actionNode.dataset.view;
        localStorage.setItem("lwcs_view_mode", state.viewMode);
        render();
        return;
    }

    if (action === "pick-file") {
        hiddenFileInput.click();
        return;
    }

    if (action === "toggle-dropzone") {
        state.showDropzone = !state.showDropzone;
        render();
        return;
    }

    if (action === "toggle-share-input") {
        state.showShareInput = !state.showShareInput;
        render();
        return;
    }

    if (action === "refresh-files") {
        loadFileList();
        return;
    }

    if (action === "cancel-upload") {
        if (state.upload.inProgress) {
            state.upload.cancelRequested = true;
            if (state.upload.abortController) {
                state.upload.abortController.abort();
            }
            render();
        }
        return;
    }

    if (action === "download-share-link") {
        downloadByShareLink();
        return;
    }

    if (action === "apply-suggestion") {
        state.searchQuery = actionNode.dataset.keyword || "";
        rememberSearch(state.searchQuery);
        render();
        return;
    }

    if (action === "toggle-select-all") {
        toggleSelectAll(event.target.checked);
        return;
    }

    if (action === "toggle-select") {
        toggleFileSelection(Number(actionNode.dataset.fileId), Number(actionNode.dataset.index), event);
        return;
    }

    if (action === "download-file") {
        downloadFile(Number(actionNode.dataset.fileId));
        return;
    }

    if (action === "open-rename") {
        openRenameModal(Number(actionNode.dataset.fileId));
        return;
    }

    if (action === "share-file") {
        createShare(Number(actionNode.dataset.fileId));
        return;
    }

    if (action === "delete-file") {
        deleteFile(Number(actionNode.dataset.fileId));
        return;
    }

    if (action === "batch-share") {
        batchShareSelected();
        return;
    }

    if (action === "batch-delete") {
        batchDeleteSelected();
        return;
    }

    if (action === "open-profile-edit") {
        state.modal = { type: "profile", payload: null };
        render();
        return;
    }

    if (action === "close-modal") {
        if (!stopCloseNode || actionNode.dataset.action === "close-modal") {
            closeModal();
        }
        return;
    }

    if (action === "copy-share-link") {
        copyShareLink();
        return;
    }

    if (action === "logout") {
        handleLogout();
        return;
    }

    if (action === "select-chat-user") {
        const inputId = actionNode.dataset.source === "quick" ? "quick-chat-user-id" : "chat-user-id";
        selectChatUser(document.getElementById(inputId)?.value);
        return;
    }

    if (action === "refresh-messages") {
        loadMessages(true);
        return;
    }

    if (action === "send-message") {
        sendMessage();
    }
}

function onDocumentInput(event) {
    if (event.target.id === "search-input") {
        state.searchQuery = event.target.value;
        render();
    }
}

function onDocumentChange(event) {
    const row = event.target.closest("[data-file-id][data-index]");
    if (row && event.target.type !== "checkbox") {
        toggleFileSelection(Number(row.dataset.fileId), Number(row.dataset.index), event);
    }
}

async function onDocumentSubmit(event) {
    const form = event.target.closest("form[data-form]");
    if (!form) {
        return;
    }

    event.preventDefault();
    const formType = form.dataset.form;
    const formData = new FormData(form);

    if (formType === "login") {
        await handleLogin(String(formData.get("username") || ""), String(formData.get("password") || ""));
        return;
    }

    if (formType === "register") {
        await handleRegister(
            String(formData.get("username") || ""),
            String(formData.get("email") || ""),
            String(formData.get("password") || "")
        );
        return;
    }

    if (formType === "rename") {
        await submitRename(String(formData.get("name") || ""));
        return;
    }

    if (formType === "profile") {
        await submitProfileUpdate(
            String(formData.get("username") || ""),
            String(formData.get("email") || "")
        );
    }
}

function onDocumentContextMenu(event) {
    const fileNode = event.target.closest("[data-file-id]");
    if (!fileNode || !state.currentToken) {
        return;
    }

    event.preventDefault();
    const fileId = Number(fileNode.dataset.fileId);
    state.contextMenu = {
        visible: true,
        x: Math.min(event.clientX, window.innerWidth - 190),
        y: Math.min(event.clientY, window.innerHeight - 180),
        fileId
    };
    render();
}

function onDocumentKeyDown(event) {
    if (event.key === "Escape") {
        hideContextMenu();
        closeModal();
    }
}

function onDragEnter(event) {
    if (!state.currentToken || !hasFiles(event)) return;
    event.preventDefault();
    event.stopPropagation();

    if (!isFileSectionActive()) {
        return;
    }

    state.dragDepth += 1;
    if (state.dragActive) return;
    state.dragActive = true;
    state.showDropzone = true;
    render();
}

function onDragOver(event) {
    if (!state.currentToken || !hasFiles(event)) return;
    event.preventDefault();
    event.stopPropagation();
    if (event.dataTransfer) {
        event.dataTransfer.dropEffect = isFileSectionActive() ? "copy" : "none";
    }
}

function onDragLeave(event) {
    if (!state.currentToken || !hasFiles(event)) return;
    event.preventDefault();
    event.stopPropagation();
    if (!isFileSectionActive()) return;

    state.dragDepth = Math.max(0, state.dragDepth - 1);
    if (state.dragDepth > 0 || !state.dragActive) return;
    state.dragActive = false;
    render();
}

function onDrop(event) {
    if (!state.currentToken || !hasFiles(event)) return;
    event.preventDefault();
    event.stopPropagation();

    state.dragDepth = 0;
    state.dragActive = false;
    if (!isFileSectionActive()) {
        showToast("请切换到文件页面后再上传", "error");
        render();
        return;
    }

    const files = Array.from(event.dataTransfer?.files || []);
    if (files.length > 0) {
        uploadFiles(files);
        return;
    }
    render();
}

function hasFiles(event) {
    return Array.from(event.dataTransfer?.types || []).includes("Files");
}

function handleHiddenFilePick(event) {
    const files = Array.from(event.target.files || []);
    if (files.length === 0) return;
    uploadFiles(files);
    event.target.value = "";
}

function isFileSectionActive() {
    return ["files", "recent", "shared"].includes(state.activeSection);
}

function toggleFileSelection(fileId, index, event) {
    const next = new Set(state.selectedIds);
    const useShift = event.shiftKey && state.lastSelectedIndex >= 0;

    if (useShift) {
        const start = Math.min(state.lastSelectedIndex, index);
        const end = Math.max(state.lastSelectedIndex, index);
        for (let i = start; i <= end; i += 1) {
            const file = state.visibleFiles[i];
            if (file) next.add(file.id);
        }
    } else if (next.has(fileId)) {
        next.delete(fileId);
    } else {
        next.add(fileId);
    }

    state.selectedIds = next;
    state.lastSelectedIndex = index;
    render();
}

function toggleSelectAll(checked) {
    state.selectedIds = checked
        ? new Set(state.visibleFiles.map((file) => file.id))
        : new Set();
    render();
}

function openRenameModal(fileId) {
    const file = state.files.find((item) => Number(item.id) === fileId);
    if (!file) return;
    state.modal = {
        type: "rename",
        payload: {
            id: fileId,
            name: file.original_filename || file.filename || ""
        }
    };
    render();
}

function closeModal() {
    if (!state.modal.type) return;
    state.modal = { type: "", payload: null };
    render();
}

function hideContextMenu() {
    if (!state.contextMenu.visible) return;
    state.contextMenu = { visible: false, x: 0, y: 0, fileId: null };
    render();
}

async function handleRegister(username, email, password) {
    if (!username || !email || !password) {
        showToast("请填写完整的注册信息", "error");
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/register`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username, email, password })
        });
        const data = await res.json();
        if (data.code === 200) {
            showToast("注册成功，请使用新账号登录");
            state.authMode = "login";
            render();
        } else {
            showToast(data.message || "注册失败", "error");
        }
    } catch (_) {
        showToast("注册请求失败", "error");
    }
}

async function handleLogin(username, password) {
    if (!username || !password) {
        showToast("请输入用户名和密码", "error");
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/login`, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ username, password })
        });
        const data = await res.json();
        if (data.code === 200) {
            state.currentToken = data.data.token;
            state.currentUser = username;
            state.currentUserId = Number(data.data.id || 0);
            localStorage.setItem("lwcs_token", state.currentToken);
            localStorage.setItem("lwcs_user", username);
            localStorage.setItem("lwcs_user_id", String(state.currentUserId));
            await bootstrapAuthorizedApp();
            showToast("登录成功");
        } else {
            showToast(data.message || "登录失败", "error");
        }
    } catch (_) {
        showToast("连接服务器失败", "error");
    }
}

function getAuthHeaders(extra = {}) {
    return {
        Authorization: `Bearer ${state.currentToken}`,
        ...extra
    };
}

async function requestJson(url, options = {}) {
    const res = await fetch(url, options);
    const data = await res.json().catch(() => ({}));

    if (res.status === 401 || data.code === 401) {
        handleUnauthorized();
        throw new Error("unauthorized");
    }

    return { res, data };
}

async function loadUserProfile() {
    try {
        const { data } = await requestJson(`${API_BASE}/user/info`, {
            headers: getAuthHeaders()
        });
        if (data.code !== 200 || !data.data) return;
        const profile = data.data;
        state.profile = {
            id: Number(profile.id || 0),
            username: profile.username || state.currentUser,
            email: profile.email || "",
            createdAt: profile.created_at || "-",
            timeoutMinutes: Number(profile.timeout_minutes || 0),
            storageUsed: Number(profile.storage_used || 0),
            storageLimit: Number(profile.storage_limit || 0)
        };
        state.currentUserId = Number(profile.id || state.currentUserId || 0);
        localStorage.setItem("lwcs_user_id", String(state.currentUserId));
        render();
    } catch (_) {
        showToast("用户资料加载失败", "error");
    }
}

async function submitProfileUpdate(username, email) {
    if (!username || !email) {
        showToast("用户名和邮箱不能为空", "error");
        return;
    }

    try {
        const { data } = await requestJson(`${API_BASE}/user/profile`, {
            method: "PUT",
            headers: getAuthHeaders({ "Content-Type": "application/json" }),
            body: JSON.stringify({ username, email })
        });
        if (data.code === 200) {
            state.currentUser = username;
            localStorage.setItem("lwcs_user", username);
            closeModal();
            await loadUserProfile();
            showToast("资料更新成功");
        } else {
            showToast(data.message || "资料更新失败", "error");
        }
    } catch (_) {
        showToast("资料更新失败", "error");
    }
}

async function loadFileList() {
    state.isLoadingFiles = true;
    state.fileError = "";
    render();

    try {
        const { data } = await requestJson(`${API_BASE}/file/list?offset=0&limit=200`, {
            headers: getAuthHeaders()
        });
        if (data.code === 200) {
            state.files = Array.isArray(data.data)
                ? data.data
                : (typeof data.data === "string" ? JSON.parse(data.data) : []);
            pruneSelection();
        } else {
            state.fileError = data.message || "文件列表加载失败";
        }
    } catch (_) {
        state.fileError = "无法连接服务器，请稍后重试。";
    } finally {
        state.isLoadingFiles = false;
        render();
    }
}

function pruneSelection() {
    const available = new Set(state.files.map((file) => Number(file.id)));
    state.selectedIds = new Set([...state.selectedIds].filter((id) => available.has(id)));
}

async function uploadFiles(files) {
    const queue = files.filter((file) => file && typeof file.name === "string");
    if (!queue.length) return;
    if (state.upload.inProgress) {
        showToast("已有上传任务进行中，请稍候", "error");
        return;
    }

    state.showDropzone = true;
    render();

    let successCount = 0;
    let failedCount = 0;
    for (let i = 0; i < queue.length; i += 1) {
        const ok = await uploadSingleFile(queue[i], i + 1, queue.length);
        if (ok) {
            successCount += 1;
        } else {
            failedCount += 1;
        }
    }

    if (successCount > 0) {
        await Promise.all([loadFileList(), loadUserProfile()]);
    }

    if (queue.length === 1) {
        showToast(successCount === 1 ? "文件上传成功" : "上传失败", successCount === 1 ? "success" : "error");
    } else if (failedCount === 0) {
        showToast(`共 ${queue.length} 个文件，已全部上传完成`);
    } else if (successCount === 0) {
        showToast(`共 ${queue.length} 个文件，上传全部失败`, "error");
    } else {
        showToast(`上传完成：成功 ${successCount}，失败 ${failedCount}`, "error");
    }
}

async function uploadSingleFile(file, index, total) {
    state.upload = {
        inProgress: true,
        progress: 0,
        filename: file.name,
        currentIndex: index,
        total,
        mode: file.size >= LARGE_FILE_THRESHOLD ? "chunk" : "single",
        statusText: file.size >= LARGE_FILE_THRESHOLD ? "分块上传中" : "普通上传中",
        cancelRequested: false,
        activeUploadId: "",
        abortController: null
    };
    render();

    try {
        if (file.size >= LARGE_FILE_THRESHOLD) {
            await uploadLargeFile(file);
        } else {
            await uploadSimpleFile(file);
        }
        return true;
    } catch (error) {
        if (state.upload.cancelRequested || error?.message === "cancelled") {
            showToast(`已取消上传：${file.name}`);
        } else {
            showToast(`上传失败：${file.name}`, "error");
        }
        return false;
    } finally {
        resetUploadState();
        render();
    }
}

function resetUploadState() {
    state.upload = {
        inProgress: false,
        progress: 0,
        filename: "",
        currentIndex: 0,
        total: 0,
        mode: "",
        statusText: "",
        cancelRequested: false,
        activeUploadId: "",
        abortController: null
    };
}

async function uploadSimpleFile(file) {
    await new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        xhr.open("POST", `${API_BASE}/file/upload`);
        xhr.setRequestHeader("Authorization", `Bearer ${state.currentToken}`);
        xhr.setRequestHeader("X-Filename", encodeURIComponent(file.name));

        xhr.upload.onprogress = (evt) => {
            if (!evt.lengthComputable) return;
            state.upload.progress = Math.round((evt.loaded / evt.total) * 100);
            render();
        };

        xhr.onload = async () => {
            try {
                const data = JSON.parse(xhr.responseText || "{}");
                if (xhr.status === 401 || data.code === 401) {
                    handleUnauthorized();
                    reject(new Error("unauthorized"));
                    return;
                }

                if (xhr.status >= 200 && xhr.status < 300 && data.code === 200) {
                    state.upload.progress = 100;
                    render();
                    resolve();
                } else {
                    reject(new Error(data.message || "上传失败"));
                }
            } catch (error) {
                reject(error);
            }
        };

        xhr.onerror = () => reject(new Error("network error"));
        xhr.send(file);
    });
}

async function uploadLargeFile(file) {
    const totalChunks = Math.ceil(file.size / CHUNK_SIZE);
    const initRes = await requestJson(`${API_BASE}/file/upload/init`, {
        method: "POST",
        headers: getAuthHeaders(),
        body: JSON.stringify({
            filename: file.name,
            total_size: file.size,
            total_chunks: totalChunks,
            mime_type: file.type || "application/octet-stream"
        })
    });

    if (initRes.data.code !== 200 || !initRes.data.data?.upload_id) {
        throw new Error(initRes.data.message || "初始化上传失败");
    }

    const uploadId = initRes.data.data.upload_id;
    state.upload.activeUploadId = uploadId;
    state.upload.statusText = `分块上传中（共 ${totalChunks} 块）`;
    render();

    const chunkQueue = Array.from({ length: totalChunks }, (_, i) => i);
    let uploadedChunks = 0;

    const uploadOneChunk = async (chunkIndex) => {
        if (state.upload.cancelRequested) {
            throw new Error("cancelled");
        }
        const start = chunkIndex * CHUNK_SIZE;
        const end = Math.min(start + CHUNK_SIZE, file.size);
        const chunkBlob = file.slice(start, end);
        const chunkBuffer = await chunkBlob.arrayBuffer();
        const hash = await calculateSHA256Hex(chunkBuffer);

        const controller = new AbortController();
        state.upload.abortController = controller;
        const res = await fetch(
            `${API_BASE}/file/upload/chunk?upload_id=${encodeURIComponent(uploadId)}&chunk_index=${chunkIndex}&chunk_hash=${hash}`,
            {
                method: "POST",
                headers: {
                    Authorization: `Bearer ${state.currentToken}`
                },
                body: chunkBlob,
                signal: controller.signal
            }
        );

        const data = await res.json().catch(() => ({}));
        if (res.status === 401 || data.code === 401) {
            handleUnauthorized();
            throw new Error("unauthorized");
        }
        if (!res.ok || data.code !== 200) {
            throw new Error(data.message || `分块上传失败: ${chunkIndex}`);
        }

        uploadedChunks += 1;
        state.upload.progress = Math.round((uploadedChunks / totalChunks) * 100);
        render();
    };

    const workers = Array.from({ length: Math.min(CHUNK_CONCURRENCY, totalChunks) }, async () => {
        while (chunkQueue.length > 0) {
            const chunkIndex = chunkQueue.shift();
            if (chunkIndex === undefined) return;
            await uploadOneChunk(chunkIndex);
        }
    });

    try {
        await Promise.all(workers);
    } catch (error) {
        if (state.upload.cancelRequested || error?.name === "AbortError" || error?.message === "cancelled") {
            await requestJson(`${API_BASE}/file/upload/cancel?upload_id=${encodeURIComponent(uploadId)}`, {
                method: "POST",
                headers: getAuthHeaders()
            }).catch(() => null);
            throw new Error("cancelled");
        }
        throw error;
    }

    const completeRes = await requestJson(`${API_BASE}/file/upload/complete?upload_id=${encodeURIComponent(uploadId)}`, {
        method: "POST",
        headers: getAuthHeaders()
    });

    if (completeRes.data.code !== 200) {
        throw new Error(completeRes.data.message || "完成上传失败");
    }

    state.upload.progress = 100;
    state.upload.statusText = "上传完成";
    render();
}

async function calculateSHA256Hex(arrayBuffer) {
    const digest = await crypto.subtle.digest("SHA-256", arrayBuffer);
    return Array.from(new Uint8Array(digest))
        .map((v) => v.toString(16).padStart(2, "0"))
        .join("");
}

async function downloadFile(fileId) {
    const file = state.files.find((item) => Number(item.id) === fileId);
    if (!file) return;

    try {
        const res = await fetch(`${API_BASE}/file/download/stream?id=${fileId}`, {
            headers: getAuthHeaders()
        });
        if (!res.ok) {
            let message = "下载失败";
            try {
                const errorPayload = await res.json();
                message = errorPayload.message || message;
            } catch (_) {
                message = await res.text() || message;
            }
            if (res.status === 401) {
                handleUnauthorized();
                return;
            }
            showToast(message, "error");
            return;
        }

        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = file.original_filename || file.filename || "download";
        document.body.appendChild(link);
        link.click();
        link.remove();
        URL.revokeObjectURL(url);
        showToast("下载已开始");
    } catch (_) {
        showToast("下载失败", "error");
    }
}

async function createShare(fileId) {
    try {
        const { data } = await requestJson(`${API_BASE}/share/create`, {
            method: "POST",
            headers: getAuthHeaders({ "Content-Type": "application/json" }),
            body: JSON.stringify({ file_id: fileId })
        });
        if (data.code === 200 && data.data) {
            const url = `${window.location.origin}${data.data.share_url}`;
            state.sharedMap[fileId] = url;
            localStorage.setItem("lwcs_shared_map", JSON.stringify(state.sharedMap));
            state.modal = { type: "share", payload: { url } };
            render();
            try {
                await copyToClipboard(url);
                showToast("分享链接已复制到剪贴板");
            } catch (_) {
                showToast("分享链接已生成");
            }
        } else {
            showToast(data.message || "分享失败", "error");
        }
    } catch (_) {
        showToast("分享请求失败", "error");
    }
}

async function deleteFile(fileId) {
    const ok = window.confirm("确定要永久删除这个文件吗？当前版本删除后不会进入回收站。");
    if (!ok) return;

    try {
        const { data } = await requestJson(`${API_BASE}/file/delete`, {
            method: "POST",
            headers: getAuthHeaders({ "Content-Type": "application/json" }),
            body: JSON.stringify({ id: fileId })
        });
        if (data.code === 200) {
            delete state.sharedMap[fileId];
            localStorage.setItem("lwcs_shared_map", JSON.stringify(state.sharedMap));
            state.selectedIds.delete(fileId);
            await Promise.all([loadFileList(), loadUserProfile()]);
            showToast("文件已删除");
        } else {
            showToast(data.message || "删除失败", "error");
        }
    } catch (_) {
        showToast("删除失败", "error");
    }
}

async function submitRename(newName) {
    const payload = state.modal.payload;
    if (!payload || !payload.id) return;

    if (!newName.trim()) {
        showToast("文件名不能为空", "error");
        return;
    }

    try {
        const { data } = await requestJson(`${API_BASE}/file/rename`, {
            method: "POST",
            headers: getAuthHeaders({ "Content-Type": "application/json" }),
            body: JSON.stringify({ id: payload.id, new_name: newName.trim() })
        });
        if (data.code === 200) {
            closeModal();
            await loadFileList();
            showToast("文件重命名成功");
        } else {
            showToast(data.message || "重命名失败", "error");
        }
    } catch (_) {
        showToast("文件重命名失败", "error");
    }
}

async function batchShareSelected() {
    const ids = [...state.selectedIds];
    if (!ids.length) {
        showToast("请先选择文件", "error");
        return;
    }

    for (const id of ids) {
        await createShare(id);
    }
}

async function batchDeleteSelected() {
    const ids = [...state.selectedIds];
    if (!ids.length) {
        showToast("请先选择文件", "error");
        return;
    }

    const ok = window.confirm(`确定删除已选择的 ${ids.length} 个文件吗？`);
    if (!ok) return;

    for (const id of ids) {
        await deleteFile(id);
    }
    state.selectedIds = new Set();
    render();
}

function rememberSearch(keyword) {
    const value = keyword.trim();
    if (!value) return;
    state.searchHistory = [value, ...state.searchHistory.filter((item) => item !== value)].slice(0, 5);
    localStorage.setItem("lwcs_search_history", JSON.stringify(state.searchHistory));
}

async function copyToClipboard(text) {
    if (navigator.clipboard && window.isSecureContext) {
        await navigator.clipboard.writeText(text);
        return;
    }

    const el = document.createElement("textarea");
    el.value = text;
    el.style.cssText = "position:fixed;opacity:0;pointer-events:none";
    document.body.appendChild(el);
    el.select();
    document.execCommand("copy");
    el.remove();
}

function getSearchSuggestions() {
    return state.searchHistory.slice(0, 5);
}

async function loadServerStatus() {
    try {
        const { data } = await requestJson(`${API_BASE}/server/status`, {
            headers: getAuthHeaders()
        });
        if (data.code === 200) {
            let payload = data.data;
            if (typeof payload === "string") {
                payload = JSON.parse(payload);
            }
            const clients = Array.isArray(payload.clients) ? payload.clients : [];
            state.serverStatus = {
                summary: `活跃连接 ${payload.active_connections ?? 0}`,
                detail: clients.length > 0 ? `共同步到 ${clients.length} 个连接节点。` : "当前没有额外的活跃连接明细。",
                clients
            };
            state.statusLastUpdated = new Date().toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit", second: "2-digit" });
        } else {
            state.serverStatus = {
                summary: "状态获取失败",
                detail: data.message || "无法读取服务器状态。",
                clients: []
            };
        }
    } catch (_) {
        state.serverStatus = {
            summary: "服务不可达",
            detail: "无法连接到服务器状态接口。",
            clients: []
        };
    } finally {
        render();
    }
}

function restoreChatState() {
    if (!state.activeChatUserId) return;
    state.messageStatus = `当前会话用户 ID：${state.activeChatUserId}`;
    startMessagePolling();
    loadMessages();
}

function startMessagePolling() {
    stopMessagePolling();
    if (!state.activeChatUserId || !state.currentToken) return;
    state.messagePollTimer = window.setInterval(() => {
        loadMessages();
    }, 3000);
}

function stopMessagePolling() {
    if (!state.messagePollTimer) return;
    window.clearInterval(state.messagePollTimer);
    state.messagePollTimer = null;
}

function startStatusPolling() {
    stopStatusPolling();
    if (!state.currentToken) return;
    state.statusPollTimer = window.setInterval(() => {
        loadServerStatus();
    }, 30000);
}

function stopStatusPolling() {
    if (!state.statusPollTimer) return;
    window.clearInterval(state.statusPollTimer);
    state.statusPollTimer = null;
}

async function selectChatUser(rawValue) {
    const value = Number(rawValue);
    if (!Number.isInteger(value) || value <= 0) {
        showToast("请输入有效的用户 ID", "error");
        return;
    }
    if (value === state.currentUserId) {
        showToast("不能给自己发消息", "error");
        return;
    }

    state.activeChatUserId = value;
    localStorage.setItem("lwcs_chat_user_id", String(value));
    state.messageStatus = `当前会话用户 ID：${value}，每 3 秒自动刷新`;
    startMessagePolling();
    await loadMessages(true);
}

async function loadMessages(showErrors = false) {
    if (!state.activeChatUserId) return;
    try {
        const { data } = await requestJson(`${API_BASE}/message/list?with_user_id=${state.activeChatUserId}&limit=50`, {
            headers: getAuthHeaders()
        });
        if (data.code === 200) {
            state.messages = Array.isArray(data.data) ? [...data.data].reverse() : [];
            state.messageStatus = `当前会话用户 ID：${state.activeChatUserId}，共 ${state.messages.length} 条消息，每 3 秒自动刷新`;
            render();
        } else if (showErrors) {
            showToast(data.message || "消息加载失败", "error");
        }
    } catch (_) {
        if (showErrors) showToast("消息加载失败", "error");
    }
}

async function sendMessage() {
    if (!state.activeChatUserId) {
        showToast("请先输入聊天对象用户 ID", "error");
        return;
    }
    const input = document.getElementById("message-input");
    const content = input?.value.trim() || "";
    if (!content) {
        showToast("消息内容不能为空", "error");
        return;
    }

    try {
        const { data } = await requestJson(`${API_BASE}/message/send`, {
            method: "POST",
            headers: getAuthHeaders({ "Content-Type": "application/json" }),
            body: JSON.stringify({ receiver_id: state.activeChatUserId, content })
        });
        if (data.code === 200) {
            if (input) input.value = "";
            showToast("消息发送成功");
            await loadMessages();
        } else {
            showToast(data.message || "消息发送失败", "error");
        }
    } catch (_) {
        showToast("消息发送失败", "error");
    }
}

async function copyShareLink() {
    const url = state.modal.payload?.url;
    if (!url) return;
    try {
        await copyToClipboard(url);
        showToast("链接已复制");
    } catch (_) {
        showToast("复制失败，请手动复制", "error");
    }
}

async function downloadByShareLink() {
    const input = document.getElementById("share-link-input");
    const raw = input?.value.trim() || "";
    if (!raw) {
        showToast("请先输入分享链接", "error");
        return;
    }

    let path = raw;
    if (raw.startsWith("http")) {
        try {
            const url = new URL(raw);
            path = `${url.pathname}${url.search}`;
        } catch (_) {
            // ignore invalid absolute url and try raw value
        }
    }

    try {
        const res = await fetch(path);
        if (!res.ok) {
            const message = await res.text();
            showToast(message || "分享下载失败", "error");
            return;
        }

        const blob = await res.blob();
        const disposition = res.headers.get("Content-Disposition") || "";
        const match = disposition.match(/filename="([^"]+)"/);
        const filename = match ? match[1] : "shared_file";
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = filename;
        document.body.appendChild(link);
        link.click();
        link.remove();
        URL.revokeObjectURL(url);
        showToast("分享文件下载成功");
    } catch (_) {
        showToast("分享下载失败", "error");
    }
}

async function handleLogout() {
    stopMessagePolling();
    try {
        if (state.currentToken) {
            await fetch(`${API_BASE}/logout`, {
                method: "POST",
                headers: getAuthHeaders()
            });
        }
    } catch (_) {
        // noop
    }
    clearAuthState();
    render();
}

function clearAuthState() {
    stopMessagePolling();
    stopStatusPolling();
    if (state.upload.abortController) {
        state.upload.abortController.abort();
    }
    state.currentToken = "";
    state.currentUser = "";
    state.currentUserId = 0;
    state.activeChatUserId = 0;
    state.profile = {
        id: 0,
        username: "",
        email: "",
        createdAt: "-",
        timeoutMinutes: 0,
        storageUsed: 0,
        storageLimit: 0
    };
    state.files = [];
    state.visibleFiles = [];
    state.messages = [];
    state.messageStatus = "请选择一个用户开始聊天";
    state.selectedIds = new Set();
    state.modal = { type: "", payload: null };
    state.contextMenu = { visible: false, x: 0, y: 0, fileId: null };
    state.dragActive = false;
    state.dragDepth = 0;
    resetUploadState();
    state.serverStatus = {
        summary: "等待同步",
        detail: "服务器状态会在登录后自动刷新。",
        clients: []
    };
    state.statusLastUpdated = "";
    localStorage.removeItem("lwcs_token");
    localStorage.removeItem("lwcs_user");
    localStorage.removeItem("lwcs_user_id");
    localStorage.removeItem("lwcs_chat_user_id");
}

function handleUnauthorized() {
    clearAuthState();
    showToast("登录已失效，请重新登录", "error");
    render();
}

function showToast(text, type = "success") {
    const id = `${Date.now()}-${Math.random()}`;
    state.toastList = [...state.toastList, { id, text, type }];
    render();
    window.setTimeout(() => {
        state.toastList = state.toastList.filter((item) => item.id !== id);
        render();
    }, 2600);
}
