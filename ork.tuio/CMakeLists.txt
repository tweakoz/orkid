project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set(CMAKE_CXX_FLAGS "-Wno-deprecated --std=c++17")

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

ADD_DEFINITIONS(-DLINUX)

target_include_directories (ork_tuio PRIVATE ${SRCD}/oscpack )
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

###################################
#install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/ork DESTINATION ${CMAKE_INSTALL_PREFIX}/include )
#install( TARGETS ork_tuio DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )

###################################
