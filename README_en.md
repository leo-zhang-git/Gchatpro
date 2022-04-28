# What is Gchatpro
Gchat is a TCP socket server used for TCP communication with multiple clients

This project is built using CMake. If you are not sure how to use it, please see the following tips

1.Execute the following two commands in the build directory of the project root directory

```
cmake ..
```
```
make
```

2.Run the generated executable file 'Gchatpro'

The server development port and the database used can be configured in conf.json in the build directory.
Create a database and run Gchatpro.sql in the build directory to generate tables that meet the requirements

## Characteristics

Use epoll + thread pool for high concurrency

The transmitted data is in the format of a user-defined packet header and JSON file

Multi-io threads Multitasking threads