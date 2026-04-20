export function renderContextMenu(state) {
    if (!state.contextMenu.visible || !state.contextMenu.fileId) {
        return "";
    }

    return `
        <div
            class="context-menu"
            style="left:${state.contextMenu.x}px;top:${state.contextMenu.y}px"
        >
            <button data-action="download-file" data-file-id="${state.contextMenu.fileId}">下载文件</button>
            <button data-action="open-rename" data-file-id="${state.contextMenu.fileId}">重命名</button>
            <button data-action="share-file" data-file-id="${state.contextMenu.fileId}">创建分享</button>
            <button data-action="delete-file" data-file-id="${state.contextMenu.fileId}" class="danger">删除文件</button>
        </div>
    `;
}
