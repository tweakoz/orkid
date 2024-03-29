project (ork CXX)
include (GenerateExportHeader)
###################################

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( UTPP_DEST_HEADERDIR $ENV{OBT_STAGE}/include/utpp )

###################################

file(GLOB src_1 ${SRCD}/*.cpp)
file(GLOB src_2 ${SRCD}/Posix/*.cpp)

###################################

add_library (ork_utpp SHARED ${src_1} ${src_2} )

ork_std_target_opts(ork_utpp)

###################################

IF(${APPLE})
    #target_include_directories (ork_utpp PRIVATE /usr/local/include)
    #target_link_libraries(ork_utpp LINK_PUBLIC m pthread)
    #target_link_libraries(ork_utpp LINK_PUBLIC
     #   "-framework AppKit"
        #"-framework IOKit"
    #)
    #target_link_libraries(ork_utpp LINK_PUBLIC objc )
ELSEIF(${UNIX})
    #target_link_libraries(ork_utpp LINK_PUBLIC rt dl pthread)
ENDIF()

target_include_directories (ork_utpp PRIVATE ${SRCD}/oscpack )

###################################

add_custom_target(mkutpphdrdir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${UTPP_DEST_HEADERDIR})
add_custom_target(mkutppposixhdrdir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${UTPP_DEST_HEADERDIR}/Posix)

add_custom_command(
  TARGET ork_utpp
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${SRCD}/*.h ${UTPP_DEST_HEADERDIR}
)

add_custom_command(
  TARGET ork_utpp
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${SRCD}/Posix/*.h ${UTPP_DEST_HEADERDIR}/Posix
)

###################################

install(TARGETS ork_utpp LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )
