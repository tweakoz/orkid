project (ork CXX)
include (GenerateExportHeader)

###################################

IF(${APPLE})
set(CMAKE_MACOSX_RPATH 1)
ENDIF()

set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )


###################################

file(GLOB src_tool ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable (ork.tool.release ${src_tool})


###################################

set( libs "" )

IF(${APPLE})
    list(APPEND libs m pthread )
    #set_target_properties( ork.tool.release PROPERTIES LINK_FLAGS -Wl,-stack_size,0x4000000,-pagezero_size,10000,-image_base,100000000 )
    #set_target_properties(ork.tool.release PROPERTIES LINK_FLAGS "-Wl,-stack_size,0x4000000,-pagezero_size 10000 -image_base 100000000 -image_base 7fff04c4a000" )
ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread )
    #set_target_properties( ork.tool.release PROPERTIES LINK_FLAGS "-Wl,--stack,0x4000000" )
ENDIF()

target_link_libraries(ork.tool.release ${libs} ork_tool ork_ecs ork_lev2 ork_core ork_utpp ork_tuio ork_fcollada )

target_include_directories (ork.tool.release PRIVATE ${COREINCD} )

install(TARGETS ork.tool.release DESTINATION $ENV{OBT_STAGE}/bin )


###################################
