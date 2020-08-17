

file(GLOB_RECURSE DIV_SRCS "${DIV_LIB_DIR}/*.c")

add_library(divlibs STATIC ${DIV_SRCS})

if (LINK_LIBS)
list(APPEND LINK_LIBS divlibs)
message("enter append mode ${LINK_LIBS}")
else ()
set(LINK_LIBS divlibs)
message("enter set mode ${LINK_LIBS}")
endif()
