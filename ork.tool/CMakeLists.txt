project (ork CXX)
include (GenerateExportHeader)

###################################

IF(${APPLE})
set(QT5BASE /usr/local/opt/qt)
set(CMAKE_MACOSX_RPATH 1)
ENDIF()

###################################

set(Qt5Widgets_DIR ${QT5BASE}/lib/cmake/Qt5Widgets)
set(Qt5Test_DIR ${QT5BASE}/lib/cmake/Qt5Test)
set(Qt5Core_DIR ${QT5BASE}/lib/cmake/Qt5Core)
set(Qt5_DIR ${QT5BASE}/lib/cmake)
set(Qt5Concurrent_DIR ${QT5BASE}/lib/cmake/Qt5Concurrent)
set(Qt5Gui_DIR ${QT5BASE}/lib/cmake/Qt5Gui)
set(Qt5Network_DIR ${QT5BASE}/lib/cmake/Qt5Network)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5X11Extras REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( LEV2INCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.lev2/inc )
set( ECSINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.ent/inc )
set( CLDABASE ${CMAKE_CURRENT_SOURCE_DIR}/../ork.fcollada305/ext/fcollada )

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
target_include_directories (ork_tool PRIVATE ${CLDABASE}/FCollada )
target_include_directories (ork_tool PRIVATE ${CLDABASE}/FCollada/LibXML/include )
target_include_directories (ork_tool PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_tool PRIVATE $ENV{OBT_STAGE}/include/bullet )
target_include_directories (ork_tool PRIVATE /usr/include/python2.7 )

###################################

set( libs ork_core ork_tuio ork_lev2 ork_ecs ork_fcollada ork_utpp )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio )

IF(${APPLE})
    target_include_directories (ork_tool PRIVATE /usr/local/include)
    list(APPEND libs m pthread )
    list(APPEND libs "-framework OpenGL" "-framework Python"  )
    list(APPEND libs "-framework CoreMIDI" "-framework CoreAudio" )
    list(APPEND libs "-framework AudioUnit" "-framework AudioToolbox" )
    list(APPEND libs "-framework Carbon" "-framework Foundation" )
    list(APPEND libs "-framework ApplicationServices" "-framework AppKit" )
    list(APPEND libs glfw.3 objc Qt5::Widgets Qt5::Core Qt5::Gui )
ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread python2.7 GL Qt5::X11Extras )
ENDIF()

target_link_libraries(ork_tool ${libs})

###################################

install(TARGETS ork_tool LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )
