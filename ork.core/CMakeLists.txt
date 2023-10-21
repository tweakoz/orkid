project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )

###################################

file(GLOB_RECURSE src_application ${SRCD}/application/*.cpp)
file(GLOB_RECURSE src_asset ${SRCD}/asset/*.cpp)
file(GLOB_RECURSE src_dataflow ${SRCD}/dataflow/*.cpp)
file(GLOB_RECURSE src_event ${SRCD}/event/*.cpp)
file(GLOB_RECURSE src_file ${SRCD}/file/*.cpp)
file(GLOB_RECURSE src_kernel ${SRCD}/kernel/*.cpp)
file(GLOB_RECURSE src_math ${SRCD}/math/*.cpp)
file(GLOB_RECURSE src_object ${SRCD}/object/*.cpp)
file(GLOB_RECURSE src_reflect ${SRCD}/reflect/*.cpp)
file(GLOB_RECURSE src_rtti ${SRCD}/rtti/*.cpp)
file(GLOB_RECURSE src_stream ${SRCD}/stream/*.cpp)
file(GLOB_RECURSE src_util ${SRCD}/util/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_hdl ${SRCD}/hdl/*.cpp)
file(GLOB_RECURSE src_python ${SRCD}/python/*.cpp)
file(GLOB_RECURSE src_test ${SRCD}/test/*.cpp)

set( orksrcs ${src_application} ${src_asset} ${src_dataflow} )
list( APPEND orksrcs ${src_event} ${src_file} ${src_kernel} )
list( APPEND orksrcs ${src_math} ${src_object} ${src_reflect} )
list( APPEND orksrcs ${src_rtti} ${src_stream} ${src_util} )
list( APPEND orksrcs ${src_gfx} ${src_hdl} ${src_python} ${src_test} )

###################################
# ISPC files (intel specific)
###################################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )

    gen_ispc_object_list( ${SRCD}/math/*.ispc
                          ork_core.dir/src/math/
                          ispc_math_object_list )

    list( APPEND orksrcs ${ispc_math_object_list} )

ENDIF()

###################################

if(NOT TARGET ObtOpenBlasLibTarget)
#error()
endif(NOT TARGET ObtOpenBlasLibTarget )

add_library (ork_core SHARED ${orksrcs} )

ork_std_target_opts(ork_core)
enable_python_on_target(ork_core)
target_link_libraries(ork_core LINK_PRIVATE ork_utpp zmq  )

target_link_libraries(ork_core LINK_PRIVATE Boost::filesystem )
target_link_libraries(ork_core LINK_PUBLIC ${ObtOpenBlas_LIBRARIES} )

target_include_directories (ork_core PRIVATE ${INCD} )

install(TARGETS ork_core LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )

###################################

add_subdirectory (pyext)
add_subdirectory (examples)
add_subdirectory (tests)
add_subdirectory (utils)
