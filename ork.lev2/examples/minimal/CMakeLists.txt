cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.lev2.gfx.example.minimal CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)
add_executable (ork.lev2.gfx.example.minimal.exe ${srcs} )

target_link_libraries(ork.lev2.gfx.example.minimal.exe LINK_PRIVATE ork_core ork_lev2 )

set_target_properties(ork.lev2.gfx.example.minimal.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.lev2.gfx.example.minimal.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.lev2.gfx.example.minimal.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.lev2.gfx.example.minimal.exe PRIVATE ${SRCD} )
target_include_directories (ork.lev2.gfx.example.minimal.exe PRIVATE $ENV{QTDIR}/include)

install(TARGETS ork.lev2.gfx.example.minimal.exe DESTINATION $ENV{OBT_STAGE}/bin )
