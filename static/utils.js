export function escapeHtml(value) {
    return String(value || "")
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

export function formatBytes(bytes) {
    if (!Number.isFinite(bytes) || bytes < 0) return "-";

    const units = ["B", "KB", "MB", "GB", "TB"];
    let value = bytes;
    let index = 0;

    while (value >= 1024 && index < units.length - 1) {
        value /= 1024;
        index += 1;
    }

    return `${value.toFixed(value >= 100 ? 0 : value >= 10 ? 1 : 2)} ${units[index]}`;
}

export function renderFileActions(fileId) {
    return `
        <button class="icon-button" data-action="download-file" data-file-id="${fileId}">下载</button>
        <button class="icon-button" data-action="open-rename" data-file-id="${fileId}">重命名</button>
        <button class="icon-button" data-action="share-file" data-file-id="${fileId}">分享</button>
        <button class="icon-button danger" data-action="delete-file" data-file-id="${fileId}">删除</button>
    `;
}
