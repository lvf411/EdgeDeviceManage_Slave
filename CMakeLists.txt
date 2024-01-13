cmake_minimum_required(VERSION 3.0.0)
project(demo)

# 指定目标系统
set(CMAKE_SYSTEM_NAME Linux)
# 指定目标平台
# set(CMAKE_SYSTEM_PROCESSOR arm)
# # 指定交叉编译工具链的根路径
# set(CROSS_CHAIN_PATH /home/lf/Downloads/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64)
# # 指定C编译器
# set(CMAKE_C_COMPILER "${CROSS_CHAIN_PATH}/bin/arm-linux-gnueabihf-gcc")
# # 指定C++编译器
# set(CMAKE_CXX_COMPILER "${CROSS_CHAIN_PATH}/bin/arm-linux-gnueabihf-g++")
# Enable C++11
set(CMAKE_CXX_STANDARD 11)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

aux_source_directory (src SRC_LIST)

#set(JSONCPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/jsoncpp/include)
#set(JSONCPP_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/include/jsoncpp/lib/libjsoncpp.so)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include
#                    ${JSONCPP_INCLUDE_DIR}
                    )

link_directories(usr/local/lib
                )

add_executable (main ${SRC_LIST})

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

#target_link_libraries(main -lpthread ${JSONCPP_LIBRARIES})
target_link_libraries(main -lpthread -ljsoncpp)



