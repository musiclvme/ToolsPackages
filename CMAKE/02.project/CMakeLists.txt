# cmake 最小版本要求
cmake_minimum_required (VERSION 3.6)

set(COMPILER_PLATFORM "arm")


# check ToolChain
if (COMPILER_PLATFORM STREQUAL "X86")
    message("compiler platform is X86")

else ()
    message("compiler platform is none-x86")
    set(CROSS_COMPILE "arm-none-eabi-")
    find_program(compiler NAMES "${CROSS_COMPILE}gcc" PATHS ENV NO_DEFAULT_PATH)
    if (compiler STREQUAL compiler-NOTFOUND)
        message(FATAL_ERROR "could not find compiler ${CROSS_COMPILE}gcc\n")
    endif ()

    #find compiler
    execute_process(COMMAND which ${CROSS_COMPILE}gcc
                    RESULT_VARIABLE GCC_RESULT_STRING
                    OUTPUT_VARIABLE GCC_OUTPUT_STRING)
    message("GCC_OUTPUT_STRING=${GCC_OUTPUT_STRING}\nGCC_RESULT_STRING=${GCC_RESULT_STRING}\n")

    if (${GCC_RESULT_STRING})
        message(FATAL_ERROR "no compiler named ${CROSS_COMPILE}gcc\n")
    else ()
        string(REPLACE "${CROSS_COMPILE}gcc\n" "" compiler_dir ${GCC_OUTPUT_STRING})
        message("cross_compiler dir is:${compiler_dir}")
    endif()

    set(CMAKE_C_COMPILER ${compiler_dir}${CROSS_COMPILE}gcc)
    set(CMAKE_AR "${compiler_dir}${CROSS_COMPILE}ar" CACHE FILEPATH "" FORCE)
	set(CMAKE_AS "${compiler_dir}${CROSS_COMPILE}as")
    set(CMAKE_LD "${compiler_dir}${CROSS_COMPILE}ld")
	SET(CMAKE_STRIP  "${compiler_dir}${CROSS_COMPILE}strip")
	set(CMAKE_C_FLAGS "-g -O3 -Os -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mabi=aapcs -Wl,--gc-sections -ffunction-sections -mthumb -nostdlib")


endif()



#指定project名字
project(Mydemo)

#打印log
message("This is a test demo")
message("CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
message("PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}")
message("PROJECT_BINARY_DIR=${PROJECT_BINARY_DIR}")


#compiler info
message("=====================compiler ==========================")
message("CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}\n")
message("CMAKE_AR = ${CMAKE_AR}\n")
message("CMAKE_AS = ${CMAKE_AS}\n")
message("CMAKE_LD = ${CMAKE_LD}\n")


#set cmake vars
#if you want to build your interface srcs, set this on
set(USE_MYOWN_LIB ON)

# notify demo.c to use your interface
if (USE_MYOWN_LIB)
set(ENABLE_INTERFACE ON)
endif ()


# generate .h from config.h.in
configure_file (
  "${PROJECT_SOURCE_DIR}/script/config.h.in"
  "${PROJECT_SOURCE_DIR}/include/config.h"
)



# sub dir
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/demo_src)


