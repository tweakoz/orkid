project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/ext/fcollada )

###################################

file(GLOB src_1 ${SRCD}/Fcollada/*.cpp)
file(GLOB src_2 ${SRCD}/Fcollada/LibXML/*.c)
file(GLOB src_3 ${SRCD}/Fcollada/FCDocument/*.cpp)
file(GLOB src_4 ${SRCD}/Fcollada/FMath/*.cpp)
file(GLOB src_5 ${SRCD}/Fcollada/FUtils/*.cpp)
file(GLOB src_6 ${SRCD}/Fcollada/Output/*.cpp)
file(GLOB src_7 ${SRCD}/Fcollada/Output/*.cpp)
file(GLOB src_8 ${SRCD}/FColladaPlugins/FArchiveXML/*.cpp)

###################################

set( SRCS ${src_1} ${src_2} ${src_3} ${src_4} ${src_5} )
list( APPEND SRCS ${src_6} ${src_7} ${src_8} )
add_library (ork_fcollada SHARED ${SRCS} )

###################################

IF(${APPLE})
    target_include_directories (ork_fcollada PRIVATE /usr/local/include)
    target_link_libraries(ork_fcollada LINK_PUBLIC m pthread)
    target_link_libraries(ork_fcollada LINK_PUBLIC
        "-framework AppKit"
        "-framework IOKit"
    )
    target_link_libraries(ork_fcollada LINK_PUBLIC objc )
ELSEIF(${UNIX})
    target_link_libraries(ork_fcollada LINK_PUBLIC rt dl pthread)
ENDIF()

target_include_directories (ork_fcollada PRIVATE ${SRCD}/FCollada )
target_include_directories (ork_fcollada PRIVATE ${SRCD}/FCollada/LibXML/include )

###################################
