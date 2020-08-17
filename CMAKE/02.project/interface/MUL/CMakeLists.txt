

file(GLOB_RECURSE MUL_SRCS "${MUL_LIB_DIR}/*.c")

add_library(mullibs STATIC ${MUL_SRCS})

if (LINK_LIBS)
list(APPEND LINK_LIBS mullibs)
message("enter append mode ${LINK_LIBS}")
else ()
set(LINK_LIBS mullibs)
message("enter set mode ${LINK_LIBS}")
endif()
