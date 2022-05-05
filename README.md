# 什么是Gchatpro
Gchat 是一个tcp socket服务器，用于和多个客户端进行tcp通信
本项目使用CMake构建，如果你不清楚如何使用，请看以下提示

1.在项目根目录下的build目录下执行下面的两条指令

```
cmake ..
```
```
make
```

2.运行生成的可执行文件Gchatpro

**其它依赖项**

build目录下的 conf.json 中可以配置服务器开发端口和所使用的数据库，自行
创建一个数据库并运行build目录下的Gchatpro.sql生成符合要求的表

cmake工具

你需要安装mysql 5.5.62版本或其兼容版本

你需要安装libmysqlclient-dev库

## 特点
使用epoll + 线程池实现高并发

传输的数据格式为自定义包头+json文件

多io线程多任务线程