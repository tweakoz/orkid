cmake_minimum_required (VERSION 3.13.4)
include(orkid)

project (ork.example.lev2.gfx.cputexstream CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)
add_executable (ork.example.lev2.gfx.cputexstream.exe ${srcs} )

ork_std_target_opts_exe(ork.example.lev2.gfx.cputexstream.exe)

target_link_libraries(ork.example.lev2.gfx.cputexstream.exe LINK_PRIVATE ork_core ork_lev2 )

set_target_properties(ork.example.lev2.gfx.cputexstream.exe PROPERTIES LINKER_LANGUAGE CXX)
#enable_memdebug_on_target(ork.example.lev2.gfx.cputexstream.exe)

target_include_directories (ork.example.lev2.gfx.cputexstream.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.example.lev2.gfx.cputexstream.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.example.lev2.gfx.cputexstream.exe PRIVATE ${SRCD} )
