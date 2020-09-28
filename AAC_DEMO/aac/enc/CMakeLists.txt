INCLUDE(${PLAT_DIR}/macro.cmake)

project(aacenc LANGUAGES CXX C ASM)

set(PATH_ENC "aac/common")
include_directories(${PLAT_DIR}/${PATH_ENC}/libFDK/include)
include_directories(${PLAT_DIR}/${PATH_ENC}/libFDK/src)
include_directories(${PLAT_DIR}/${PATH_ENC}/libSYS/include)
include_directories(${PLAT_DIR}/${PATH_ENC}/libSYS/src)
include_directories(${PLAT_DIR}/${PATH_ENC}/libPCMutils/include)
include_directories(${PLAT_DIR}/${PATH_ENC}/libPCMutils/src)

include_directories(${PROJECT_SOURCE_DIR}/libAACenc/include)
include_directories(${PROJECT_SOURCE_DIR}/libAACenc/src)
include_directories(${PROJECT_SOURCE_DIR}/libMpegTPEnc/include)
include_directories(${PROJECT_SOURCE_DIR}/libMpegTPEnc/src)
include_directories(${PROJECT_SOURCE_DIR}/libSBRenc/include)
include_directories(${PROJECT_SOURCE_DIR}/libSBRenc/src)

aux_source_directory(libAACenc/src SOURCES)
aux_source_directory(libMpegTPEnc/src SOURCES)
aux_source_directory(libSBRenc/src SOURCES)
aux_source_directory(${PLAT_DIR}/${PATH_ENC}/libFDK/src SOURCES)
aux_source_directory(${PLAT_DIR}/${PATH_ENC}/libSYS/src SOURCES)
aux_source_directory(${PLAT_DIR}/${PATH_ENC}/libPCMutils/src SOURCES)

set (TARGET_NAME codec${PROJECT_NAME})

add_library(${TARGET_NAME}  STATIC ${SOURCES})
