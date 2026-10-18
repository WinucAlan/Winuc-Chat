# Winuc-Chat
一个基于C++作为后端写成的轻量聊天网站，可在断网情况下使用，使用sqlite3和cpp-httplib库。

## 编译与运行
此处给出MinGW下编译方法。
### Windows
请确保`mingw64/bin`已经被添加至`PATH`环境变量。
```cmd
g++ main.cpp -o main -lws2_32 -Ivendor/sqlite3 -Ivendor/cpp-httplib -Ivendor/nlohmann-json
```
### Linux
#### 1.安装g++
```bash
sudo apt install g++
```
#### 2.配置SQLite环境（可选）
```bash
sudo apt install libsqlite3-dev
```
#### 3.编译
如果执行了第二步，则执行
```bash
g++ main.cpp -o main -lsqlite3 -Ivendor/cpp-httplib -Ivendor/nlohmann-json
```
否则执行
```bash
g++ main.cpp -o main -Ivendor/sqlite3 -Ivendor/cpp-httplib -Ivendor/nlohmann-json
```