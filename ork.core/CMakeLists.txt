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

set( orksrcs ${src_application} ${src_asset} ${src_dataflow} )
list( APPEND orksrcs ${src_event} ${src_file} ${src_kernel} )
list( APPEND orksrcs ${src_math} ${src_object} ${src_reflect} )
list( APPEND orksrcs ${src_rtti} ${src_stream} ${src_util} )
list( APPEND orksrcs ${src_gfx} )

###################################

add_library (ork_core SHARED ${orksrcs})

###################################

IF(${APPLE})
    target_include_directories (ork_core PRIVATE /usr/local/include)
    target_link_libraries(ork_core LINK_PRIVATE m pthread)
    target_link_libraries(ork_core LINK_PRIVATE
        "-framework AppKit"
        "-framework IOKit"
    )
    target_link_libraries(ork_core LINK_PRIVATE objc boost_filesystem  boost_system)
ELSEIF(${UNIX})
    target_link_libraries(ork_core LINK_PRIVATE rt dl pthread boost_filesystem boost_system)
ENDIF()

target_include_directories (ork_core PRIVATE ${INCD} )

install(TARGETS ork_core LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )

###################################

add_subdirectory (tests)
