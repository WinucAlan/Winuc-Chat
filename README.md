# Winuc-Chat
一个基于C++作为后端写成的轻量聊天网站，可在断网情况下使用，使用sqlite3和cpp-httplib库。

## 编译与运行
### Windows
请确保`mingw64/bin`已经被添加至`PATH`环境变量。本平台下的SHA256计算基于Windows CNG (`bcrypt.h`)，不需要OpenSSL。
#### 1.编译sqlite3.c
```cmd
gcc -c vendor/sqlite3/sqlite3.c -o sqlite3.o -O2
```
#### 2.编译主程序
```cmd
g++ -std=c++17 -O2 -Isrc -Ivendor/cpp-httplib -Ivendor/nlohmann-json -Ivendor/sqlite3 src/main.cpp sqlite3.o -o src/main.exe -lws2_32 -lbcrypt
cd src
main
```
### Linux
本平台下的SHA256计算基于OpenSSL，故需要安装OpenSSL。
#### 1.安装g++
```bash
sudo apt install gcc g++
```
#### 2.配置SQLite环境（可选）
```bash
sudo apt install libsqlite3-dev
```
#### 3.安装OpenSSL
```bash
sudo apt install libssl-dev
```
#### 4.编译
如果执行了第二步，则执行
```bash
g++ -std=c++17 -O2 -Isrc -Ivendor/cpp-httplib -Ivendor/nlohmann-json src/main.cpp -o src/main -lsqlite3 -lcrypto -ldl -lpthread
cd src
./main
```
否则执行
```bash
gcc -c vendor/sqlite3/sqlite3.c -o sqlite3.o -O2
```
```bash
g++ -std=c++17 -O2 -Isrc -Ivendor/cpp-httplib -Ivendor/nlohmann-json -Ivendor/sqlite3 src/main.cpp sqlite3.o -o src/main -ldl -lpthread -lcrypto
cd src
./main
```
