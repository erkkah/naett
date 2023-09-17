project(naett)

cmake_minimum_required(VERSION 3.4.1)

set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS} -Wall -Werror")
add_library(naett STATIC ../../../../../../naett.c ../../../../../test.c)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wall -Werror")
add_library(native-activity SHARED naettactivity.cpp)
target_include_directories(native-activity PRIVATE ../../../../../../)

# add lib dependencies
target_link_libraries(native-activity
    naett
    android
    log)
