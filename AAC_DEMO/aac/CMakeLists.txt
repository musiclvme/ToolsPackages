INCLUDE(${PLAT_DIR}/macro.cmake)
                                           
project(aac LANGUAGES CXX C ASM)
 
add_subdirectory(${PROJECT_SOURCE_DIR}/enc)
add_subdirectory(${PROJECT_SOURCE_DIR}/dec)

