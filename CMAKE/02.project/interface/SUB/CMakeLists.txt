

file(GLOB_RECURSE SUB_SRCS "${SUB_LIB_DIR}/*.c")

add_library(sublibs STATIC ${SUB_SRCS})

if (LINK_LIBS)
list(APPEND LINK_LIBS sublibs)
message("enter append mode ${LINK_LIBS}")
else ()
set(LINK_LIBS sublibs)
message("enter set mode ${LINK_LIBS}")
endif()
