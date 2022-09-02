cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
#enableQt5()
project (ork.osxhmdenum CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( EXE ork.osxhmdenum)
set( APP ork.osxhmdenum.app)
set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )

file(GLOB testsrcs ${TESTSRCD}/*.mm)
add_executable (${EXE} ${testsrcs} )
ork_std_target_opts_exe(${EXE})
target_link_libraries(${EXE} LINK_PRIVATE ork_utpp )
target_link_libraries(${EXE} LINK_PRIVATE ork_core )
target_link_libraries(${EXE} LINK_PRIVATE ork_lev2 )
target_link_libraries(${EXE} LINK_PRIVATE ork_ecs )
target_link_libraries(${EXE} LINK_PRIVATE boost_system )

set_target_properties(${EXE} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (${EXE} PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (${EXE} PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (${EXE} PRIVATE ${SRCD} )
target_include_directories (${EXE} PRIVATE $ENV{QTDIR}/include)
#target_link_options(${EXE} PUBLIC -Wl,-rpath-link=${PYSIDE_LIBRARY_PATH} )
target_link_libraries(${EXE} PRIVATE "-framework ModelIO")
target_link_libraries(${EXE} PRIVATE "-framework Metal")
target_link_libraries(${EXE} PRIVATE "-framework MetalKit")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -ObjC++")



add_custom_command( OUTPUT ${APP} POST_BUILD
                    COMMAND ${TESTSRCD}/build_bundle.py )

add_custom_target(AppBundle ALL
        DEPENDS ${APP} ${EXE}
        )
