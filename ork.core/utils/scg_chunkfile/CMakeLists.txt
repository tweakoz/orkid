cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
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
add_executable (ork.scg_chunkfile.exe ${srcs} )

target_link_libraries(ork.scg_chunkfile.exe LINK_PRIVATE ork_core )
target_link_libraries(ork.scg_chunkfile.exe LINK_PRIVATE ork_lev2 )
target_link_libraries(ork.scg_chunkfile.exe LINK_PRIVATE boost_system )

set_target_properties(ork.scg_chunkfile.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.scg_chunkfile.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.scg_chunkfile.exe PRIVATE ${SRCD} )

ork_std_target_opts_exe(ork.scg_chunkfile.exe)
