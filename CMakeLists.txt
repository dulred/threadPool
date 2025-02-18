cmake_minimum_required(VERSION 3.10)

project(threadPool)

include_directories(.)


add_executable(main main.cpp threadPool/threadpool.cpp)
