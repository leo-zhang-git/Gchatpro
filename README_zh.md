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


## 特点
使用epoll + 线程池实现高并发

传输的数据格式为自定义包头+json文件

多io线程多任务线程