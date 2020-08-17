

file(GLOB_RECURSE ADD_SRCS "${ADD_LIB_DIR}/*.c")




set(ADD_HEADER_SRC "add.h")
set(ADD_HEADER_INC "add.h")
add_custom_command(OUTPUT ${ADD_HEADER_INC}
  COMMAND echo "Generating addlibs header file..."
  COMMAND ${CMAKE_COMMAND} -E copy ${ADD_LIB_DIR}/${ADD_HEADER_SRC} ${CMAKE_CURRENT_BINARY_DIR}/${ADD_HEADER_INC}
)

add_library(addlibs STATIC ${ADD_SRCS} ${ADD_HEADER_INC})

if (LINK_LIBS)
list(APPEND LINK_LIBS addlibs)
message("enter append mode ${LINK_LIBS}")
else ()
set(LINK_LIBS addlibs)
message("enter set mode ${LINK_LIBS}")
endif()
