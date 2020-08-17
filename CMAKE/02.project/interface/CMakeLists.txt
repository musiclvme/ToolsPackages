

if (USE_MYOWN_LIB)
set(ADD_LIB_DIR ${MY_LIB_DIR}/ADD)
set(SUB_LIB_DIR ${MY_LIB_DIR}/SUB)
set(MUL_LIB_DIR ${MY_LIB_DIR}/MUL)
set(DIV_LIB_DIR ${MY_LIB_DIR}/DIV)

#include dir set
set(LIB_INCLUDE_DIR ${ADD_LIB_DIR} ${SUB_LIB_DIR} ${MUL_LIB_DIR} ${DIV_LIB_DIR})


include(${ADD_LIB_DIR}/CMakeLists.txt)
include(${SUB_LIB_DIR}/CMakeLists.txt)
include(${MUL_LIB_DIR}/CMakeLists.txt)
include(${DIV_LIB_DIR}/CMakeLists.txt)

else ()
message("nothing to do for interface\n")
endif()
