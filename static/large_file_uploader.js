class LargeFileUploader {
    constructor(options = {}) {
        this.baseUrl = options.baseUrl || '/api';
        this.chunkSize = options.chunkSize || 4 * 1024 * 1024;
        this.concurrency = options.concurrency || 3;
        this.callbacks = options.callbacks || {};
    }

    async initUpload(file) {
        const totalChunks = Math.ceil(file.size / this.chunkSize);
        const response = await fetch(`${this.baseUrl}/file/upload/init`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'X-Token': localStorage.getItem('session_token')
            },
            body: JSON.stringify({
                filename: file.name,
                total_size: file.size,
                total_chunks: totalChunks,
                mime_type: file.type || 'application/octet-stream'
            })
        });
        return await response.json();
    }

    async calculateSHA256(data) {
        const buffer = await crypto.subtle.digest('SHA-256', data);
        const hashArray = Array.from(new Uint8Array(buffer));
        return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
    }

    async uploadChunk(uploadId, chunkIndex, chunkData) {
        const hash = await this.calculateSHA256(chunkData);
        const params = new URLSearchParams({
            upload_id: uploadId,
            chunk_index: chunkIndex,
            chunk_hash: hash
        });
        
        const response = await fetch(
            `${this.baseUrl}/file/upload/chunk?${params}`,
            {
                method: 'POST',
                headers: { 'X-Token': localStorage.getItem('session_token') },
                body: chunkData
            }
        );
        return await response.json();
    }

    async completeUpload(uploadId) {
        const response = await fetch(
            `${this.baseUrl}/file/upload/complete?upload_id=${uploadId}`,
            { method: 'POST', headers: { 'X-Token': localStorage.getItem('session_token') } }
        );
        return await response.json();
    }

    async upload(file) {
        const init = await this.initUpload(file);
        const uploadId = init.data.upload_id;
        
        for (let i = 0; i < Math.ceil(file.size / this.chunkSize); i++) {
            const start = i * this.chunkSize;
            const end = Math.min(start + this.chunkSize, file.size);
            const chunk = file.slice(start, end);
            await this.uploadChunk(uploadId, i, await chunk.arrayBuffer());
        }
        
        return await this.completeUpload(uploadId);
    }
}
