cmake_minimum_required(VERSION 3.2)
project(mmlib  LANGUAGES CXX C ASM)

file(TO_CMAKE_PATH ${PROJECT_SOURCE_DIR} PLAT_DIR)

include(${PLAT_DIR}/macro.cmake)

set(CMAKE_SKIP_RPATH TRUE)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall  -Wno-unused-function  -g")
set(CMAKE_C_FLAGS ${CMAKE_CXX_FLAGS})
## target library path
set(LIBRARY_OUTPUT_PATH   ${PROJECT_BINARY_DIR}/lib)
link_directories(${LIBRARY_OUTPUT_PATH})

add_subdirectory(${PLAT_DIR}/aac)


include_directories(${PLAT_DIR}/aac/common/libFDK/include)
include_directories(${PLAT_DIR}/aac/common/libPCMutils/include)
include_directories(${PLAT_DIR}/aac/common/libSYS/include)
include_directories(${PLAT_DIR}/aac/dec/libAACdec/include)
include_directories(${PLAT_DIR}/aac/enc/libAACenc/include)


file(GLOB_RECURSE SRC_DEC_BIN "${PLAT_DIR}/aac_test/dec_test/*.c")
set(TARGET_DEC dec_test)
add_executable(${TARGET_DEC} ${SRC_DEC_BIN})
target_link_libraries(${TARGET_DEC} codecaacdec)

file(GLOB_RECURSE SRC_ENC_BIN "${PLAT_DIR}/aac_test/enc_test/*.c")
set(TARGET_ENC enc_test)
add_executable(${TARGET_ENC} ${SRC_ENC_BIN})
target_link_libraries(${TARGET_ENC} codecaacenc)

