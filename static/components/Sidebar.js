const navItems = [
    { id: "files", label: "全部文件", hint: "集中管理上传内容", badge: "All" },
    { id: "recent", label: "最近", hint: "按最近上传排序", badge: "New" },
    { id: "shared", label: "我的分享", hint: "查看已创建链接", badge: "Link" },
    { id: "messages", label: "消息", hint: "点对点消息通信", badge: "Chat" }
];

export function renderSidebar(state) {
    const storagePercent = state.profile.storageLimit > 0
        ? Math.min(100, Math.round((state.profile.storageUsed / state.profile.storageLimit) * 100))
        : 0;

    return `
        <aside class="sidebar">
            <div class="sidebar-brand">
                <div class="brand-mark">LC</div>
                <div>
                    <strong>LightComm Cloud</strong>
                    <p>文件与消息一体化工作台</p>
                </div>
            </div>

            <nav class="sidebar-nav">
                ${navItems.map((item) => `
                    <button
                        class="sidebar-nav-item ${state.activeSection === item.id ? "is-active" : ""}"
                        data-action="switch-section"
                        data-section="${item.id}"
                    >
                        <span class="sidebar-nav-badge">${item.badge}</span>
                        <span class="sidebar-nav-copy">
                            <strong>${item.label}</strong>
                            <small>${item.hint}</small>
                        </span>
                    </button>
                `).join("")}
            </nav>

            <section class="sidebar-panel">
                <div class="sidebar-panel-header">
                    <strong>空间概览</strong>
                    <span>${storagePercent}%</span>
                </div>
                <div class="storage-meter">
                    <div class="storage-meter-fill" style="width:${storagePercent}%"></div>
                </div>
                <p>${state.formatBytes(state.profile.storageUsed)} / ${state.formatBytes(state.profile.storageLimit)}</p>
            </section>

            <section class="sidebar-panel">
                <div class="sidebar-panel-header">
                    <strong>当前账号</strong>
                </div>
                <p>${state.profile.username || state.currentUser || "-"}</p>
                <small>${state.profile.email || "未同步邮箱"}</small>
                <button class="ghost-button full-width" data-action="open-profile-edit">编辑资料</button>
                <button class="ghost-button full-width" data-action="logout">退出登录</button>
            </section>

            <section
                class="sidebar-panel sidebar-panel--clickable ${state.activeSection === "status" ? "is-active" : ""}"
                data-action="switch-section"
                data-section="status"
            >
                <div class="sidebar-panel-header">
                    <strong>系统状态</strong>
                    ${state.statusLastUpdated ? `<small>${state.statusLastUpdated}</small>` : ""}
                </div>
                <p>${state.serverStatus.summary}</p>
                <small>${state.serverStatus.detail}</small>
            </section>
        </aside>
    `;
}
