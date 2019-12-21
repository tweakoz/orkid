project (ork_test CXX)

set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/../inc )

link_directories(${CMAKE_INSTALL_PREFIX}/lib)
set( destbin $ENV{ORKDOTBUILD_STAGE_DIR}/bin/ )
set( destlib $ENV{ORKDOTBUILD_STAGE_DIR}/lib/ )
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
if(${APPLE})
set(CMAKE_MACOSX_RPATH 1)
include_directories(AFTER /usr/local/include)
endif()
include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)


message(${proj_unittestpp_SOURCE_DIR}/src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.core.test ${testsrcs} )

target_link_libraries(ork.core.test LINK_PRIVATE ork_utpp )
target_link_libraries(ork.core.test LINK_PRIVATE ork_core )
target_link_libraries(ork.core.test LINK_PRIVATE boost_filesystem boost_system)

set_target_properties(ork.core.test PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.core.test PRIVATE ${INCD} )

install(TARGETS ork.core.test DESTINATION $ENV{OBT_STAGE}/bin )

# copy to staging bin folder (so we can run without installing)
#add_custom_command(TARGET ork_test POST_BUILD
#                   COMMAND ${CMAKE_COMMAND} -E copy
#                   $<TARGET_FILE:ork_test> ${destbin} )
