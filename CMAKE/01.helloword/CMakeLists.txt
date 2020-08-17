# cmake 最小版本要求
cmake_minimum_required (VERSION 3.6)

#指定project名字
project(Mydemo)

#打印log
message("This is a test demo")
message("CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")

#定义变量DEMO_SRC, 表示需要编译的源码文件
file(GLOB_RECURSE DEMO_SRC "${CMAKE_SOURCE_DIR}/src/*.c")


#根据前面定义的DEMO_SRC,生成可执行文件demo
add_executable(demo ${DEMO_SRC})


#target依赖的头文件目录定义
target_include_directories(demo PUBLIC ${CMAKE_SOURCE_DIR})
