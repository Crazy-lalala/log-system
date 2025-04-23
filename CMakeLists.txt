cmake_minimum_required(VERSION 3.12)

project(logger LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fPIC -g -Wall -O2")



include_directories(${PROJECT_SOURCE_DIR}/code/include/)
aux_source_directory(${PROJECT_SOURCE_DIR}/code/source/ SRC_LIST)

add_library(logger_shared SHARED ${SRC_LIST})
set_target_properties(logger_shared PROPERTIES
    OUTPUT_NAME "logger"
    # VERSION 1.0
    # SOVERSION 1
    #LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
)

add_library(logger_static STATIC ${SRC_LIST})
set_target_properties(logger_static PROPERTIES
    OUTPUT_NAME "logger"
    #ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib"
)

target_link_libraries(logger_shared pthread)
target_link_libraries(logger_static pthread)

add_executable(demo code/demo/main.cpp)

target_link_libraries(demo logger_shared)