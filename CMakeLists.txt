cmake_minimum_required(VERSION 3.0.0)
project(demo)

# 指定目标系统
set(CMAKE_SYSTEM_NAME Linux)
# 指定目标平台
set(CMAKE_SYSTEM_PROCESSOR arm)

# Enable C++11
set(CMAKE_CXX_STANDARD 11)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

aux_source_directory (src SRC_LIST)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include
                    )

link_directories(usr/local/lib
                )

add_executable (main ${SRC_LIST})

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

target_link_libraries(main -lpthread -ljsoncpp)




