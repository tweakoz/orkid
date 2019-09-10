project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set(CMAKE_CXX_FLAGS "-Wno-deprecated --std=c++17")

###################################

file(GLOB src_1 ${SRCD}/*.cpp)
file(GLOB src_2 ${SRCD}/Posix/*.cpp)

###################################

add_library (ork_utpp SHARED ${src_1} ${src_2} )

###################################

IF(${APPLE})
    target_include_directories (ork_utpp PRIVATE /usr/local/include)
    target_link_libraries(ork_utpp LINK_PUBLIC m pthread)
    target_link_libraries(ork_utpp LINK_PUBLIC
        "-framework AppKit"
        "-framework IOKit"
    )
    target_link_libraries(ork_utpp LINK_PUBLIC objc )
ELSEIF(${UNIX})
    target_link_libraries(ork_utpp LINK_PUBLIC rt dl pthread)
ENDIF()

ADD_DEFINITIONS(-DLINUX)

target_include_directories (ork_utpp PRIVATE ${SRCD}/oscpack )
link_directories(${CMAKE_INSTALL_PREFIX}/lib)

###################################
#install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/ork DESTINATION ${CMAKE_INSTALL_PREFIX}/include )
#install( TARGETS ork_utpp DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )

###################################
