project (ent_test CXX)

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

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )

message(${proj_unittestpp_SOURCE_DIR}/src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.lev2.test ${testsrcs} )

target_link_libraries(ork.lev2.test LINK_PRIVATE ork_utpp )
target_link_libraries(ork.lev2.test LINK_PRIVATE ork_core )
target_link_libraries(ork.lev2.test LINK_PRIVATE ork_lev2 )
target_link_libraries(ork.lev2.test LINK_PRIVATE ork_ecs )

set_target_properties(ork.lev2.test PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.lev2.test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ork.core/inc )
target_include_directories (ork.lev2.test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ork.lev2/inc )
target_include_directories (ork.lev2.test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../ork.ent/inc )
target_include_directories (ork.lev2.test PRIVATE $ENV{OBT_STAGE}/include/luajit-2.0 )
target_include_directories (ork.lev2.test PRIVATE ${SRCD} )

install(TARGETS ork.lev2.test DESTINATION $ENV{OBT_STAGE}/bin )

# copy to staging bin folder (so we can run without installing)
#add_custom_command(TARGET ork_test POST_BUILD
#                   COMMAND ${CMAKE_COMMAND} -E copy
#                   $<TARGET_FILE:ork_test> ${destbin} )
