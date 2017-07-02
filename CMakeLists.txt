cmake_minimum_required(VERSION 3.7)
project(CBLinuxN)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES CloudBedlamNative.cpp json11.cpp)

set(CMAKE_EXE_LINKER_FLAGS "-lpthread -pthread" CACHE INTERNAL "" FORCE)

add_executable(CBLinuxN ${SOURCE_FILES})