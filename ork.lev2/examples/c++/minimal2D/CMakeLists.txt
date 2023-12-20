cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.lev2.gfx.minimal2D CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)
add_executable (ork.example.lev2.gfx.minimal2D.exe ${srcs} )

ork_std_target_opts_exe(ork.example.lev2.gfx.minimal2D.exe)
target_link_libraries(ork.example.lev2.gfx.minimal2D.exe LINK_PRIVATE ork_core ork_lev2 )

set_target_properties(ork.example.lev2.gfx.minimal2D.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.example.lev2.gfx.minimal2D.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.example.lev2.gfx.minimal2D.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.example.lev2.gfx.minimal2D.exe PRIVATE ${SRCD} )
