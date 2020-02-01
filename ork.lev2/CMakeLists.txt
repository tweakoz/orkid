project (ork CXX)
include (GenerateExportHeader)

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( TUIOINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.tuio/inc )
###################################

file(GLOB_RECURSE src_aud ${SRCD}/aud/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_input ${SRCD}/input/*.cpp)
file(GLOB_RECURSE src_qtui ${SRCD}/qtui/*.cpp)
file(GLOB_RECURSE src_ui ${SRCD}/ui/*.cpp)
file(GLOB_RECURSE src_ovr ${SRCD}/vr/*.cpp)
file(GLOB src_lev2 ${SRCD}/*.cpp)

IF(${APPLE})
  file(GLOB src_apple ${SRCD}/gfx/gl/*.mm)
ELSE()
  set( src_apple )
ENDIF()

file(GLOB_RECURSE src_glad ${SRCD}/gfx/gl/glad/*.c)

set( orksrcs ${src_aud} ${src_gfx} ${src_input} )
list( APPEND orksrcs ${src_qtui} ${src_ui} ${src_lev2} )
list( APPEND orksrcs ${src_apple} ${src_ovr} ${src_glad} )

list( APPEND orksrcs ${INCD}/ork/lev2/ezapp.h )

###################################

add_library (ork_lev2 SHARED ${orksrcs})

###################################

set( libs ork_core ork_tuio )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio )

IF(${APPLE})
    target_include_directories (ork_lev2 PRIVATE /usr/local/include)
    list(APPEND libs m pthread boost_filesystem )
    list(APPEND libs "-framework OpenGL"  )
    list(APPEND libs "-framework CoreMIDI" "-framework CoreAudio" )
    list(APPEND libs "-framework AudioUnit" "-framework AudioToolbox" )
    list(APPEND libs "-framework Carbon" "-framework Foundation" )
    list(APPEND libs "-framework ApplicationServices" "-framework AppKit" )
    list(APPEND libs glfw.3 objc)
ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread GL Qt5::X11Extras openvr_api Xrandr )
ENDIF()

list(APPEND libs Qt5::Widgets Qt5::Core Qt5::Gui)

target_link_libraries(ork_lev2 ${libs})

target_include_directories (ork_lev2 PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_lev2 PRIVATE ${INCD} )
target_include_directories (ork_lev2 PRIVATE ${COREINCD} )
target_include_directories (ork_lev2 PRIVATE ${TUIOINCD} )
target_include_directories (ork_lev2 PRIVATE $ENV{QTDIR}/include/QtGui/$ENV{QTVER})

###################################

install(TARGETS ork_lev2 LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )

###################################

add_subdirectory (tests)
add_subdirectory (examples)
