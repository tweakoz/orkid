project (ork CXX)
include (GenerateExportHeader)

enableQt5()

###################################

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( LEV2INCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.lev2/inc )
set( ECSINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.ent/inc )
set( CLDABASE $ENV{OBT_STAGE}/include/fcollada )

###################################

file(GLOB_RECURSE src_ent ${SRCD}/ent/*.cpp)
file(GLOB_RECURSE src_filter ${SRCD}/filter/*.cpp)
file(GLOB_RECURSE src_ged ${SRCD}/ged/*.cpp)
file(GLOB_RECURSE src_qtui ${SRCD}/qtui/*.cpp)
file(GLOB_RECURSE src_test ${SRCD}/test/*.cpp)
file(GLOB_RECURSE src_toolcore ${SRCD}/toolcore/*.cpp)
file(GLOB src_tool ${SRCD}/*.cpp)

set( orksrcs ${src_ent} ${src_filter} ${src_ged} ${src_test} )
list( APPEND orksrcs ${src_qtui} ${src_tool} ${src_toolcore} )

###################################

set( mocsrcs ${INCD}/orktool/qtui/qtmainwin.h )
list( APPEND mocsrcs ${INCD}/orktool/qtui/qtconsole.h )
list( APPEND mocsrcs ${INCD}/orktool/qtui/qtapp.h )
list( APPEND mocsrcs ${INCD}/orktool/ged/ged.h )

list( APPEND orksrcs ${mocsrcs} )

###################################

add_library (ork_tool SHARED ${orksrcs})

target_include_directories (ork_tool PRIVATE ${INCD} )
target_include_directories (ork_tool PRIVATE ${SRCD} )
target_include_directories (ork_tool PRIVATE ${COREINCD} )
target_include_directories (ork_tool PRIVATE ${LEV2INCD} )
target_include_directories (ork_tool PRIVATE ${ECSINCD} )
target_include_directories (ork_tool PRIVATE ${CLDABASE} )
target_include_directories (ork_tool PRIVATE ${CLDABASE}/LibXML/include )
target_include_directories (ork_tool PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_tool PRIVATE $ENV{OBT_STAGE}/include/bullet )
target_include_directories (ork_tool PRIVATE $ENV{OBT_STAGE}/include/python3.8d )

target_include_directories (ork_tool PRIVATE ${Python_INCLUDE_DIRS} )

###################################

set( libs ork_core ork_tuio ork_lev2 ork_ecs ork_fcollada ork_utpp python3.8d )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio )

list(APPEND libs Qt5::Widgets Qt5::Core Qt5::Gui)
IF(${APPLE})
    target_include_directories (ork_tool PRIVATE /usr/local/include)
    list(APPEND libs m pthread )
    list(APPEND libs "-framework OpenGL" )
    list(APPEND libs "-framework CoreMIDI" "-framework CoreAudio" )
    list(APPEND libs "-framework AudioUnit" "-framework AudioToolbox" )
    list(APPEND libs "-framework Carbon" "-framework Foundation" )
    list(APPEND libs "-framework ApplicationServices" "-framework AppKit" )
    list(APPEND libs glfw.3 objc assimp )

ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread GL Qt5::X11Extras assimp  )
ENDIF()

target_link_libraries(ork_tool PRIVATE ${libs})

###################################

install(TARGETS ork_tool LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )
