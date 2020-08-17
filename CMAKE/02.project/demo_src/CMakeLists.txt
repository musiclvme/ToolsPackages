


#include dependencies
#不同目录不能使用add_subdirectory

set(TOP_DIR ${CMAKE_SOURCE_DIR})
set(MY_LIB_DIR ${TOP_DIR}/interface)
include(${MY_LIB_DIR}/CMakeLists.txt)


# my demo srcs
file(GLOB_RECURSE DEMO_SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_executable(mydemo ${DEMO_SRC})
target_link_libraries(mydemo ${LINK_LIBS})


# Includes
#config.h
target_include_directories(mydemo PUBLIC ${PROJECT_SOURCE_DIR}/include)

#interface
target_include_directories(mydemo PUBLIC ${LIB_INCLUDE_DIR})
