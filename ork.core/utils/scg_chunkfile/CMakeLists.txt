cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (scg_chunkfile CXX)

###
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
set( destbin $ENV{ORKDOTBUILD_STAGE_DIR}/bin/ )
set( destlib $ENV{ORKDOTBUILD_STAGE_DIR}/lib/ )
set( ORKROOT $ENV{ORKID_WORKSPACE_DIR} )
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
if(${APPLE})
set(CMAKE_MACOSX_RPATH 1)
include_directories(AFTER /usr/local/include)
endif()
include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)

setupLev2EXE(ork.scg_chunkfile.exe ${srcs} )

target_include_directories (ork.scg_chunkfile.exe PRIVATE ${SRCD} )
