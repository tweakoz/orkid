project (ork CXX)
include (GenerateExportHeader)
###################################

set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc/tuio )
set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set(CMAKE_CXX_FLAGS "-Wno-deprecated --std=c++17")
#set( TUIO_DEST_HEADERDIR $ENV{OBT_STAGE}/include/tuio )
#set( OSCPACK_DEST_HEADERDIR ${TUIO_DEST_HEADERDIR}/oscpack )
#set( OSC_DEST_HEADERDIR ${OSCPACK_DEST_HEADERDIR}/osc )
#set( IP_DEST_HEADERDIR ${OSCPACK_DEST_HEADERDIR}/ip )
###################################

file(GLOB src_1 ${SRCD}/*.cpp)
file(GLOB src_2 ${SRCD}/oscpack/*.cpp)
file(GLOB src_3 ${SRCD}/oscpack/ip/posix/*.cpp)
file(GLOB src_4 ${SRCD}/oscpack/osc/*.cpp)

###################################

add_library (ork_tuio SHARED ${src_1} ${src_2} ${src_3} ${src_4} )

###################################

ork_std_target_set_defs(ork_tuio)
get_property( TGT_DEFINITIIONS TARGET ork_tuio PROPERTY TGT_DEFINITIIONS )
target_compile_definitions(ork_tuio PRIVATE ${TGT_DEFINITIIONS} )

###################################

target_include_directories (ork_tuio PRIVATE ${INCD} )
target_include_directories (ork_tuio PRIVATE ${INCD}/oscpack )
target_include_directories (ork_tuio PRIVATE ${INCD}/oscpack/osc )
target_include_directories (ork_tuio PRIVATE ${INCD}/oscpack/ip )

IF(${APPLE})
    target_include_directories (ork_tuio PRIVATE /usr/local/include)
    target_link_libraries(ork_tuio LINK_PUBLIC m pthread)
    target_link_libraries(ork_tuio LINK_PUBLIC
        "-framework AppKit"
        "-framework IOKit"
    )
    target_link_libraries(ork_tuio LINK_PUBLIC objc )
ELSEIF(${UNIX})
    target_link_libraries(ork_tuio LINK_PUBLIC rt dl pthread)
ENDIF()

link_directories(${CMAKE_INSTALL_PREFIX}/lib)

###################################

install( TARGETS ork_tuio DESTINATION $ENV{OBT_STAGE}/lib )

###################################


