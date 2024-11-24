cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (vdbviewer CXX)

###
link_directories(${CMAKE_INSTALL_PREFIX}/lib)
include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER /usr/include)
include_directories(AFTER /usr/include/drm)

set( destbin $ENV{ORKDOTBUILD_STAGE_DIR}/bin/ )
set( destlib $ENV{ORKDOTBUILD_STAGE_DIR}/lib/ )
set( ORKROOT $ENV{ORKID_WORKSPACE_DIR} )
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)


file(GLOB srcs ./*.cpp)
add_executable (ork.vdb.viewer.exe ${srcs} )

target_link_libraries(ork.vdb.viewer.exe LINK_PRIVATE ork_core ork_lev2 openvdb )

set_target_properties(ork.vdb.viewer.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.vdb.viewer.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.vdb.viewer.exe PRIVATE ${ORKROOT}/ork.lev2/inc )

ork_std_target_opts_exe(ork.vdb.viewer.exe)
