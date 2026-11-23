cmake_minimum_required(VERSION 3.22)
project(ys_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network HttpServer Sql)

add_executable(ys-server
  src/main.cpp
  src/JobStore.cpp
  src/JobWorker.cpp
  src/ApiServer.cpp
)
target_link_libraries(ys-server PRIVATE Qt6::Core Qt6::Network Qt6::HttpServer Qt6::Sql)
target_compile_options(ys-server PRIVATE -Wall -Wextra)
