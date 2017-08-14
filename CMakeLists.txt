cmake_minimum_required(VERSION 3.7)
project(CBLinuxN)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES CloudBedlamNative.cpp json11.cpp)

set(CMAKE_EXE_LINKER_FLAGS "-lpthread -pthread" CACHE INTERNAL "" FORCE)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/chaos.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

file(COPY "Bash" DESTINATION "${CMAKE_BINARY_DIR}")

add_executable(CBLinuxN ${SOURCE_FILES})
