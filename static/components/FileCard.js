import { escapeHtml, renderFileActions } from "../utils.js";

export function renderFileCard(file, options) {
    const isSelected = options.selectedIds.has(file.id);
    const sharedTag = options.sharedMap[file.id] ? '<span class="meta-chip">已分享</span>' : "";
    const thumbnail = file.thumbnail
        ? `<img src="${file.thumbnail}" alt="${escapeHtml(file.name)}">`
        : `<div class="file-thumb-icon">${file.extensionLabel}</div>`;

    return `
        <article
            class="file-card ${isSelected ? "is-selected" : ""}"
            data-file-id="${file.id}"
            data-index="${options.index}"
        >
            <label class="selection-box">
                <input
                    type="checkbox"
                    ${isSelected ? "checked" : ""}
                    data-action="toggle-select"
                    data-file-id="${file.id}"
                    data-index="${options.index}"
                >
            </label>
            <div class="file-card-thumb">${thumbnail}</div>
            <div class="file-card-main">
                <div class="file-card-title-row">
                    <h3 title="${escapeHtml(file.name)}">${file.name}</h3>
                    <div class="file-inline-actions">
                        ${renderFileActions(file.id)}
                    </div>
                </div>
                <div class="file-card-meta">
                    <span>${file.extensionLabel}</span>
                    <span>${file.sizeLabel}</span>
                    <span>${file.dateLabel}</span>
                    ${sharedTag}
                </div>
            </div>
        </article>
    `;
}
