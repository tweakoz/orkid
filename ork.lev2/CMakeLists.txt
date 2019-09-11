project (ork CXX)
include (GenerateExportHeader)

###################################

IF(${APPLE})
set(QT5BASE /usr/local/opt/qt/lib/cmake)
set(CMAKE_MACOSX_RPATH 1)
ELSE()
set(QT5BASE /usr/lib/x86_64-linux-gnu/cmake)
ENDIF()

###################################

set(Qt5Widgets_DIR ${QT5BASE}/Qt5Widgets)
set(Qt5Test_DIR ${QT5BASE}/Qt5Test)
set(Qt5Core_DIR ${QT5BASE}/Qt5Core)
set(Qt5_DIR ${QT5BASE})
set(Qt5Concurrent_DIR ${QT5BASE}/Qt5Concurrent)
set(Qt5Gui_DIR ${QT5BASE}/Qt5Gui)
set(Qt5Network_DIR ${QT5BASE}/Qt5Network)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Gui REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
###################################

file(GLOB_RECURSE src_aud ${SRCD}/aud/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_input ${SRCD}/input/*.cpp)
file(GLOB_RECURSE src_qtui ${SRCD}/qtui/*.cpp)
file(GLOB_RECURSE src_ui ${SRCD}/ui/*.cpp)
file(GLOB src_lev2 ${SRCD}/*.cpp)

IF(${APPLE})
  file(GLOB src_apple ${SRCD}/gfx/gl/*.mm)
ELSE()
  set( src_apple )
ENDIF()

set( orksrcs ${src_aud} ${src_gfx} ${src_input} )
list( APPEND orksrcs ${src_qtui} ${src_ui} ${src_lev2} )
list( APPEND orksrcs ${src_apple} )

###################################

add_library (ork_lev2 SHARED ${orksrcs})

###################################

set( libs ork_core ork_tuio )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio )

IF(${APPLE})
    target_include_directories (ork_lev2 PRIVATE /usr/local/include)
    list(APPEND libs m pthread )
    list(APPEND libs "-framework OpenGL"  )
    list(APPEND libs "-framework CoreMIDI" "-framework CoreAudio" )
    list(APPEND libs "-framework AudioUnit" "-framework AudioToolbox" )
    list(APPEND libs "-framework Carbon" "-framework Foundation" )
    list(APPEND libs "-framework ApplicationServices" "-framework AppKit" )
    list(APPEND libs glfw.3 objc)
ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread GL )
ENDIF()

list(APPEND libs Qt5::Widgets Qt5::Core Qt5::Gui)

target_link_libraries(ork_lev2 ${libs})

target_include_directories (ork_lev2 PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_lev2 PRIVATE ${INCD} )
target_include_directories (ork_lev2 PRIVATE ${COREINCD} )

###################################
