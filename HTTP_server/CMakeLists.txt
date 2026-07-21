cmake_minimum_required(VERSION 3.10)
project(HttpServer CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/utils
)

add_library(Socket STATIC src/server/Socket.cpp)
target_link_libraries(Socket pthread)

add_library(Epoll STATIC src/server/Epoll.cpp)
target_link_libraries(Epoll pthread)

add_library(HttpRequest STATIC src/server/HttpRequest.cpp)

add_library(HttpResponse STATIC src/server/HttpResponse.cpp)

add_library(FileUtil STATIC src/utils/FileUtil.cpp)

add_library(HttpServer STATIC src/server/HttpServer.cpp)
target_link_libraries(HttpServer Socket Epoll HttpRequest HttpResponse FileUtil pthread)

add_executable(http_server src/main.cpp)
target_link_libraries(http_server HttpServer pthread)