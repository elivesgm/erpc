#!/bin/bash

/opt/compiler/gcc-8.2/bin/g++ -std=c++11 main.cpp epoll_server.cpp -o main -lpthread
