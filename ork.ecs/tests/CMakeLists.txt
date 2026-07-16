cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.ecs.test CXX)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)


set( EXE ork.test.ecs.exe )
setupEcsEXE(${EXE} ${testsrcs} )
target_include_directories (${EXE} PRIVATE ${SRCD} )
target_include_directories (${EXE} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
target_include_directories (${EXE} PRIVATE $ENV{OBT_STAGE}/include/bullet ) # bullet_terrain_reload.cpp pulls physics/bullet_impl.h
target_link_libraries(${EXE} LINK_PRIVATE ork_utpp )
target_link_libraries(${EXE} LINK_PRIVATE BulletCollision BulletDynamics LinearMath ) # bullet_terrain_reload.cpp
