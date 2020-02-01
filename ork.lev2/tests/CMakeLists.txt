cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.lev2.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
message(${proj_unittestpp_SOURCE_DIR}/src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.lev2.test.exe ${testsrcs} )

target_link_libraries(ork.lev2.test.exe LINK_PRIVATE ork_utpp )
target_link_libraries(ork.lev2.test.exe LINK_PRIVATE ork_core )
target_link_libraries(ork.lev2.test.exe LINK_PRIVATE ork_lev2 )
target_link_libraries(ork.lev2.test.exe LINK_PRIVATE ork_ecs )
target_link_libraries(ork.lev2.test.exe LINK_PRIVATE boost_system )

set_target_properties(ork.lev2.test.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.lev2.test.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.lev2.test.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.lev2.test.exe PRIVATE ${SRCD} )
target_include_directories (ork.lev2.test.exe PRIVATE $ENV{QTDIR}/include)

install(TARGETS ork.lev2.test.exe DESTINATION $ENV{OBT_STAGE}/bin )
