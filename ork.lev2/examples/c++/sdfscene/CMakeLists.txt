cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.example.lev2.gfx.sdfscene CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)
add_executable (ork.example.lev2.gfx.sdfscene.exe ${srcs} )

ork_std_target_opts_exe(ork.example.lev2.gfx.sdfscene.exe)

target_link_libraries(ork.example.lev2.gfx.sdfscene.exe LINK_PRIVATE ork_core ork_lev2 tbb )

set_target_properties(ork.example.lev2.gfx.sdfscene.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.example.lev2.gfx.sdfscene.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.example.lev2.gfx.sdfscene.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.example.lev2.gfx.sdfscene.exe PRIVATE ${SRCD} )
