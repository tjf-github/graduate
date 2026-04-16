#!/usr/bin/env python3
import requests
import hashlib
import argparse
import sys
from pathlib import Path

class LargeFileUploadTester:
    def __init__(self, server_url, session_token, chunk_size=4*1024*1024):
        self.server_url = server_url.rstrip('/')
        self.session_token = session_token
        self.chunk_size = chunk_size
        self.headers = {'X-Token': session_token}

    def init_upload(self, filename, total_size):
        total_chunks = (total_size + self.chunk_size - 1) // self.chunk_size
        response = requests.post(
            f'{self.server_url}/api/file/upload/init',
            json={'filename': filename, 'total_size': total_size, 'total_chunks': total_chunks},
            headers=self.headers
        )
        return response.json().get('data')

    def upload_chunk(self, upload_id, chunk_index, chunk_data):
        chunk_hash = hashlib.sha256(chunk_data).hexdigest()
        response = requests.post(
            f'{self.server_url}/api/file/upload/chunk',
            params={'upload_id': upload_id, 'chunk_index': chunk_index, 'chunk_hash': chunk_hash},
            data=chunk_data,
            headers=self.headers
        )
        return response.json()

    def complete_upload(self, upload_id):
        response = requests.post(
            f'{self.server_url}/api/file/upload/complete',
            params={'upload_id': upload_id},
            headers=self.headers
        )
        return response.json()

    def upload_file(self, filepath):
        file_path = Path(filepath)
        if not file_path.exists():
            print(f"File not found: {filepath}")
            return False
        
        upload_info = self.init_upload(file_path.name, file_path.stat().st_size)
        upload_id = upload_info['upload_id']
        
        with open(filepath, 'rb') as f:
            chunk_index = 0
            while True:
                chunk = f.read(self.chunk_size)
                if not chunk:
                    break
                self.upload_chunk(upload_id, chunk_index, chunk)
                chunk_index += 1
        
        result = self.complete_upload(upload_id)
        print(f"Upload successful: {result}")
        return True

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--file', required=True)
    parser.add_argument('--server', default='http://localhost:8080')
    parser.add_argument('--token', required=True)
    args = parser.parse_args()
    
    tester = LargeFileUploadTester(args.server, args.token)
    success = tester.upload_file(args.file)
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
