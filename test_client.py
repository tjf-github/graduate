#!/usr/bin/env python3
"""
云盘系统测试客户端
用于测试API接口
"""

import requests
import json
import os
import sys

class CloudDiskClient:
    def __init__(self, base_url="http://localhost:8080"):
        self.base_url = base_url
        self.token = None
        
    def register(self, username, email, password):
        """用户注册"""
        url = f"{self.base_url}/api/register"
        data = {
            "username": username,
            "email": email,
            "password": password
        }
        response = requests.post(url, json=data)
        print(f"Register: {response.status_code}")
        print(response.json())
        return response.json()
    
    def login(self, username, password):
        """用户登录"""
        url = f"{self.base_url}/api/login"
        data = {
            "username": username,
            "password": password
        }
        response = requests.post(url, json=data)
        result = response.json()
        print(f"Login: {response.status_code}")
        print(result)
        
        if result.get('success') and 'data' in result:
            self.token = result['data'].get('token')
            print(f"Token: {self.token}")
        
        return result
    
    def logout(self):
        """用户登出"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/logout"
        headers = {"Authorization": f"Bearer {self.token}"}
        response = requests.post(url, headers=headers)
        print(f"Logout: {response.status_code}")
        print(response.json())
        self.token = None
    
    def get_user_info(self):
        """获取用户信息"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/user/info"
        headers = {"Authorization": f"Bearer {self.token}"}
        response = requests.get(url, headers=headers)
        print(f"User Info: {response.status_code}")
        print(json.dumps(response.json(), indent=2))
        return response.json()
    
    def upload_file(self, file_path):
        """上传文件"""
        if not self.token:
            print("Not logged in")
            return
        
        if not os.path.exists(file_path):
            print(f"File not found: {file_path}")
            return
        
        filename = os.path.basename(file_path)
        url = f"{self.base_url}/api/file/upload"
        headers = {
            "Authorization": f"Bearer {self.token}",
            "X-Filename": filename
        }
        
        with open(file_path, 'rb') as f:
            data = f.read()
        
        response = requests.post(url, headers=headers, data=data)
        print(f"Upload: {response.status_code}")
        print(response.json())
        return response.json()
    
    def list_files(self, offset=0, limit=10):
        """获取文件列表"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/file/list?offset={offset}&limit={limit}"
        headers = {"Authorization": f"Bearer {self.token}"}
        response = requests.get(url, headers=headers)
        print(f"File List: {response.status_code}")
        print(json.dumps(response.json(), indent=2))
        return response.json()
    
    def download_file(self, file_id, save_path):
        """下载文件"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/file/download/{file_id}"
        headers = {"Authorization": f"Bearer {self.token}"}
        response = requests.get(url, headers=headers)
        
        if response.status_code == 200:
            with open(save_path, 'wb') as f:
                f.write(response.content)
            print(f"Downloaded to: {save_path}")
        else:
            print(f"Download failed: {response.status_code}")
    
    def delete_file(self, file_id):
        """删除文件"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/file/delete/{file_id}"
        headers = {"Authorization": f"Bearer {self.token}"}
        response = requests.delete(url, headers=headers)
        print(f"Delete: {response.status_code}")
        print(response.json())
        return response.json()
    
    def rename_file(self, file_id, new_filename):
        """重命名文件"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/file/rename"
        headers = {"Authorization": f"Bearer {self.token}"}
        data = {
            "file_id": file_id,
            "new_filename": new_filename
        }
        response = requests.put(url, headers=headers, json=data)
        print(f"Rename: {response.status_code}")
        print(response.json())
        return response.json()
    
    def search_files(self, keyword):
        """搜索文件"""
        if not self.token:
            print("Not logged in")
            return
        
        url = f"{self.base_url}/api/file/search?keyword={keyword}"
        headers = {"Authorization": f"Bearer {self.token}"}
        response = requests.get(url, headers=headers)
        print(f"Search: {response.status_code}")
        print(json.dumps(response.json(), indent=2))
        return response.json()


def main():
    """主函数 - 交互式测试"""
    client = CloudDiskClient()
    
    print("=================================")
    print("  云盘系统测试客户端")
    print("=================================")
    
    while True:
        print("\n请选择操作:")
        print("1. 注册")
        print("2. 登录")
        print("3. 登出")
        print("4. 获取用户信息")
        print("5. 上传文件")
        print("6. 文件列表")
        print("7. 下载文件")
        print("8. 删除文件")
        print("9. 重命名文件")
        print("10. 搜索文件")
        print("0. 退出")
        
        choice = input("\n输入选项: ").strip()
        
        try:
            if choice == '1':
                username = input("用户名: ")
                email = input("邮箱: ")
                password = input("密码: ")
                client.register(username, email, password)
            
            elif choice == '2':
                username = input("用户名: ")
                password = input("密码: ")
                client.login(username, password)
            
            elif choice == '3':
                client.logout()
            
            elif choice == '4':
                client.get_user_info()
            
            elif choice == '5':
                file_path = input("文件路径: ")
                client.upload_file(file_path)
            
            elif choice == '6':
                offset = input("偏移量 [0]: ").strip() or "0"
                limit = input("限制数量 [10]: ").strip() or "10"
                client.list_files(int(offset), int(limit))
            
            elif choice == '7':
                file_id = input("文件ID: ")
                save_path = input("保存路径: ")
                client.download_file(int(file_id), save_path)
            
            elif choice == '8':
                file_id = input("文件ID: ")
                client.delete_file(int(file_id))
            
            elif choice == '9':
                file_id = input("文件ID: ")
                new_filename = input("新文件名: ")
                client.rename_file(int(file_id), new_filename)
            
            elif choice == '10':
                keyword = input("搜索关键词: ")
                client.search_files(keyword)
            
            elif choice == '0':
                print("退出")
                break
            
            else:
                print("无效选项")
        
        except Exception as e:
            print(f"错误: {e}")
            import traceback
            traceback.print_exc()


if __name__ == "__main__":
    main()
