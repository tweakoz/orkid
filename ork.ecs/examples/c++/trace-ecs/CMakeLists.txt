cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.ecs.trace CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

#####################################################################################

function(generate_executable the_target the_sources)
add_executable (${the_target} ${the_sources} )
ork_std_target_opts_exe(${the_target})
target_link_libraries(${the_target} LINK_PRIVATE ork_core ork_lev2 ork_ecs )
set_target_properties(${the_target} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.ecs/inc )
target_include_directories (${the_target} PRIVATE ${SRCD} )
target_include_directories (${the_target} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
endfunction()

#####################################################################################

generate_executable (ork.example.ecs.tracewriter.exe main_writer.cpp )
generate_executable (ork.example.ecs.tracereader.exe main_reader.cpp )

