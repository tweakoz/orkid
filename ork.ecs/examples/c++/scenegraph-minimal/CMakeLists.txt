cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.ecs.scenegraph-minimal CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)

set( EXE_NAME ork.example.ecs.scenegraph-minimal.exe )

add_executable (${EXE_NAME} ${srcs} )

ork_std_target_opts_exe(${EXE_NAME})
target_link_libraries(${EXE_NAME} LINK_PRIVATE ork_core ork_lev2 ork_ecs )

set_target_properties(${EXE_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.ecs/inc )
target_include_directories (${EXE_NAME} PRIVATE ${SRCD} )
target_include_directories (${EXE_NAME} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
