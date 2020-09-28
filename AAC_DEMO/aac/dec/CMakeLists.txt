INCLUDE(${PLAT_DIR}/macro.cmake)

project(aacdec LANGUAGES CXX C ASM)

include_directories(${PLAT_DIR}/aac/common/libFDK/include)
include_directories(${PLAT_DIR}/aac/common/libFDK/src)
include_directories(${PLAT_DIR}/aac/common/libSYS/include)
include_directories(${PLAT_DIR}/aac/common/libSYS/src)
include_directories(${PLAT_DIR}/aac/common/libPCMutils/include)
include_directories(${PLAT_DIR}/aac/common/libPCMutils/src)

include_directories(${PROJECT_SOURCE_DIR}/libAACdec/include)
include_directories(${PROJECT_SOURCE_DIR}/libAACdec/src)
include_directories(${PROJECT_SOURCE_DIR}/libMpegTPDec/include)
include_directories(${PROJECT_SOURCE_DIR}/libMpegTPDec/src)
include_directories(${PROJECT_SOURCE_DIR}/libSBRdec/include)
include_directories(${PROJECT_SOURCE_DIR}/libSBRdec/src)

aux_source_directory(libAACdec/src SOURCES)
aux_source_directory(libMpegTPDec/src SOURCES)
aux_source_directory(libSBRdec/src SOURCES)
aux_source_directory(${PLAT_DIR}/aac/common/libFDK/src SOURCES)
aux_source_directory(${PLAT_DIR}/aac/common/libSYS/src SOURCES)
aux_source_directory(${PLAT_DIR}/aac/common/libPCMutils/src SOURCES)

set (TARGET_NAME codec${PROJECT_NAME})

add_library(${TARGET_NAME}  STATIC ${SOURCES})
