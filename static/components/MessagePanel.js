import { escapeHtml } from "../utils.js";

export function renderMessagePanel(state, { compact = false } = {}) {
    const messages = compact ? state.messages.slice(-4) : state.messages;

    return `
        <section class="chat-panel">
            <div class="${compact ? "chat-header" : "section-toolbar"}">
                <div>
                    <strong>${compact ? "快捷消息入口" : "消息通信"}</strong>
                    <p>${state.messageStatus}</p>
                </div>
                <button class="ghost-button" data-action="${compact ? "switch-section" : "refresh-messages"}" ${compact ? 'data-section="messages"' : ""}>
                    ${compact ? "打开完整会话" : "刷新消息"}
                </button>
            </div>
            <div class="chat-toolbar">
                <input
                    id="${compact ? "quick-chat-user-id" : "chat-user-id"}"
                    type="number"
                    min="1"
                    placeholder="输入对方用户 ID"
                    value="${state.activeChatUserId || ""}"
                >
                <button class="primary-button" data-action="select-chat-user" ${compact ? 'data-source="quick"' : ""}>进入会话</button>
            </div>
            <div class="message-list">
                ${renderMessageBubbles(messages, state.currentUserId)}
            </div>
            ${compact ? "" : `
                <div class="composer-row">
                    <textarea id="message-input" placeholder="输入消息内容，系统会每 3 秒拉取最新消息"></textarea>
                    <button class="primary-button" data-action="send-message">发送消息</button>
                </div>
            `}
        </section>
    `;
}

function renderMessageBubbles(messages, currentUserId) {
    if (!messages.length) {
        return '<div class="server-status-item"><strong>暂无会话内容</strong><p>输入一个用户 ID 后即可开始收发消息。</p></div>';
    }

    return messages.map((msg) => `
        <div class="message-bubble ${Number(msg.sender_id) === currentUserId ? "self" : ""}">
            <div class="message-meta">发送方 ${msg.sender_id} -> 接收方 ${msg.receiver_id} | ${msg.created_at || "-"} | ${msg.is_read ? "已读" : "未读"}</div>
            <div>${escapeHtml(msg.content || "")}</div>
        </div>
    `).join("");
}
