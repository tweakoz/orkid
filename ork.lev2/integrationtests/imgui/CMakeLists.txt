cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.lev2.test.imgui CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER ${ORKROOT}/ork.core/inc )
include_directories(AFTER ${ORKROOT}/ork.lev2/inc )
include_directories(AFTER ${SRCD} )

set(target ork.test.lev2.imgui.exe)

add_executable (${target} 
  main.cpp
  imgui_demo.cpp
 )
ork_std_target_opts_exe(${target})
target_link_libraries(${target} LINK_PRIVATE ork_core ork_lev2 glfw )
set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
