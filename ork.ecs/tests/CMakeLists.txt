cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.ecs.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.test.ecs.exe ${testsrcs} )
ork_std_target_opts_exe(ork.test.ecs.exe)

target_link_libraries(ork.test.ecs.exe LINK_PRIVATE ork_utpp )
target_link_libraries(ork.test.ecs.exe LINK_PRIVATE ork_core )
target_link_libraries(ork.test.ecs.exe LINK_PRIVATE ork_lev2 )
target_link_libraries(ork.test.ecs.exe LINK_PRIVATE ork_ecs )

set_target_properties(ork.test.ecs.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.test.ecs.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.test.ecs.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.test.ecs.exe PRIVATE ${ORKROOT}/ork.ecs/inc )
target_include_directories (ork.test.ecs.exe PRIVATE ${SRCD} )
target_include_directories (ork.test.ecs.exe PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
