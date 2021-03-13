cmake_minimum_required(VERSION 3.17)
project(AutoTest)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)

add_executable(AutoTest main.cpp)
target_link_libraries(AutoTest pthread)