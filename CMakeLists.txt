cmake_minimum_required(VERSION 3.10)

project (BlitzenZeroApplication)

#Blitzen requires c++17 because of VkBootstrap
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(BlitzenZeroApplication src/Source.cpp)

add_subdirectory(BlitzenEngine)

target_link_libraries(BlitzenZeroApplication PUBLIC BlitzenEngine)

target_include_directories(BlitzenZeroApplication PUBLIC
                            "${PROJECT_SOURCE_DIR}/BlitzenEngine/src")