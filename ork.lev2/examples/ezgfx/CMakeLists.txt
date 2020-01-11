cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (colortest CXX)

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
add_executable (colortest.exe ${srcs} )

target_link_libraries(colortest.exe LINK_PRIVATE ork_core )
target_link_libraries(colortest.exe LINK_PRIVATE ork_lev2 )
target_link_libraries(colortest.exe LINK_PRIVATE boost_system )
target_link_libraries(colortest.exe LINK_PRIVATE Qt5::Widgets Qt5::Core Qt5::Gui)

set_target_properties(colortest.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (colortest.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (colortest.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (colortest.exe PRIVATE ${SRCD} )
target_include_directories (colortest.exe PRIVATE $ENV{QTDIR}/include)

install(TARGETS colortest.exe DESTINATION $ENV{OBT_STAGE}/bin )
