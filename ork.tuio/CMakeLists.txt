project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set(CMAKE_CXX_FLAGS "-Wno-deprecated --std=c++17")
set( TUIO_DEST_HEADERDIR $ENV{OBT_STAGE}/include/tuio )
set( OSCPACK_DEST_HEADERDIR ${TUIO_DEST_HEADERDIR}/oscpack )
set( OSC_DEST_HEADERDIR ${OSCPACK_DEST_HEADERDIR}/osc )
set( IP_DEST_HEADERDIR ${OSCPACK_DEST_HEADERDIR}/ip )
###################################

file(GLOB src_1 ${SRCD}/*.cpp)
file(GLOB src_2 ${SRCD}/oscpack/*.cpp)
file(GLOB src_3 ${SRCD}/oscpack/ip/posix/*.cpp)
file(GLOB src_4 ${SRCD}/oscpack/osc/*.cpp)

###################################

add_library (ork_tuio SHARED ${src_1} ${src_2} ${src_3} ${src_4} )

###################################

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

target_include_directories (ork_tuio PRIVATE ${SRCD}/oscpack )
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

###################################
#install( TARGETS ork_tuio DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )

###################################

add_custom_target(mktuiohdrdir ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${TUIO_DEST_HEADERDIR})
add_custom_target(mkoschdrdir ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${OSC_DEST_HEADERDIR})
add_custom_target(mkiphdrdir ALL COMMAND ${CMAKE_COMMAND} -E make_directory ${IP_DEST_HEADERDIR})

file(GLOB tuiohdrs "${SRCD}/*.h")
file(GLOB oschdrs "${SRCD}/oscpack/osc/*.h")
file(GLOB iphdrs "${SRCD}/oscpack/ip/*.h")

add_custom_command(TARGET ork_tuio POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ork_tuio> $ENV{OBT_STAGE}/lib )
add_custom_command(TARGET ork_tuio POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${tuiohdrs} ${TUIO_DEST_HEADERDIR} )
add_custom_command(TARGET ork_tuio POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${oschdrs} ${TUIO_DEST_HEADERDIR}/oscpack/osc)
add_custom_command(TARGET ork_tuio POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${iphdrs} ${TUIO_DEST_HEADERDIR}/oscpack/ip)

