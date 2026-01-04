project (orklev2 CXX)
include (GenerateExportHeader)

#find_package(embree 3 REQUIRED)
#find_package(CGAL REQUIRED)
#find_package(igl REQUIRED)
#find_package(ftxui REQUIRED)

#set( LIBIGL_DIR $ENV{OBT_STAGE}/builds/igl )
#set( LIBIGL_INCLUDE_DIR ${LIBIGL_DIR}/igl/include )
#set( CGAL_INCLUDE_DIR ${LIBIGL_DIR}/.build/_deps/cgal-src/include )

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( FTXUIINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.ftxui/include )
###################################

file(GLOB_RECURSE src_editor ${SRCD}/editor/*.cpp)
file(GLOB_RECURSE src_aud ${SRCD}/aud/*.cpp)
file(GLOB_RECURSE src_midi ${SRCD}/midi/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_input ${SRCD}/input/*.cpp)
file(GLOB_RECURSE src_glfw ${SRCD}/glfw/*.cpp)
file(GLOB_RECURSE src_ui ${SRCD}/ui/*.cpp)
file(GLOB_RECURSE src_ovr ${SRCD}/vr/*.cpp)
file(GLOB src_lev2 ${SRCD}/*.cpp)

IF(${APPLE})
  file(GLOB src_apple ${SRCD}/gfx/gl/*.mm ${SRCD}/glfw/*.mm ${SRCD}/gfx/*.mm ${SRCD}/gfx/misc/*.mm ${SRCD}/gfx/vulkan/*.mm)
  set( src_drm )
ELSE()
  set( src_apple )
  file(GLOB_RECURSE src_drm ${SRCD}/drm/*.cpp)
ENDIF()

set( orksrcs ${src_aud} ${src_midi} ${src_gfx} ${src_input} )
list( APPEND orksrcs ${src_glfw} ${src_ui} ${src_lev2} )
list( APPEND orksrcs ${src_apple} ${src_ovr} ${src_editor} ${src_drm} )
list( APPEND orksrcs ${INCD}/ork/lev2/ezapp.h )

###################################
# ISPC files (intel specific)
###################################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )

  gen_ispc_object_list( ${SRCD}/aud/*.ispc
                        ork_lev2.dir/src/aud
                        ispc_audio_object_list )

  gen_ispc_object_list( ${SRCD}/gfx/*.ispc
                        ork_lev2.dir/src/gfx
                        ispc_gfx_object_list )

  list( APPEND orksrcs ${ispc_audio_object_list} )
  list( APPEND orksrcs ${ispc_gfx_object_list} )
  
ENDIF()

###################################

add_library (ork_lev2 SHARED ${orksrcs} )

###################################

ork_std_target_opts(ork_lev2)

set( libs ork_core )
list(APPEND libs OpenImageIO OpenImageIO_Util )
list(APPEND libs portaudio rtmidi rnnoise )
list(APPEND libs avformat avcodec swscale swresample avutil )
list(APPEND libs ConvexDecomposition Bullet3Geometry LinearMath )

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
  list(APPEND libs ispc_texcomp  )
ENDIF()

target_compile_definitions( ork_lev2 PRIVATE ENABLE_VULKAN ENABLE_GLFW )
#target_include_directories (ork_lev2 PRIVATE $ENV{OBT_STAGE}/include)
#set(ENV{PATH}=$ENV{PATH}:${OBT_VULKAN_ROOT}/bin)
#set(ENV{VK_LAYER_PATH}=$ENV{OBT_VULKAN_ROOT}/etc/vulkan/explicit_layer.d)

IF(${APPLE})

    #set(ENV{LD_LIBRARY_PATH}=$ENV{LD_LIBRARY_PATH}:${OBT_VULKAN_ROOT}/dylib/macOS)
    #set(MOLTENVK_STATIC_LIB $ENV{VULKAN_SDK}/../../Debug/MoltenVK/static/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a)
    #list(APPEND libs ${MOLTENVK_STATIC_LIB} )
    #list(APPEND libs $ENV{VULKAN_SDKO}/lib/libvulkan.dylib )
    list(APPEND libs vulkan )
    #target_compile_definitions(ork_lev2 PRIVATE USE_MOLTENVK_DIRECT)
    #target_link_directories( ork_lev2 PRIVATE $ENV{VULKAN_SDKO}/frameworks )
    list(APPEND libs "-framework OpenGL" "-framework Metal" "-framework IOSurface"  )
    list(APPEND libs "-framework CoreMIDI" "-framework CoreAudio" "-framework QuartzCore" )
    list(APPEND libs "-framework AudioUnit" "-framework AudioToolbox" )
    list(APPEND libs "-framework Carbon" "-framework Foundation" )
    list(APPEND libs "-framework ApplicationServices" "-framework AppKit" )
    list(APPEND libs "-framework Accelerate")
    list(APPEND libs "-framework AVFoundation" "-framework CoreMedia" "-framework CoreVideo" "-framework VideoToolbox")
    list(APPEND libs glfw.3)
ELSEIF(${UNIX})

    set(ENV{LD_LIBRARY_PATH}=$ENV{LD_LIBRARY_PATH}:${OBT_VULKAN_ROOT}/lib)

    list(APPEND libs GL Xrandr glfw drm input udev )
    target_include_directories (ork_lev2 PRIVATE ${ObtPipewireSpaInclude_DIR} ${ObtPipewireInclude_DIR} )
    target_link_libraries(ork_lev2 LINK_PRIVATE ${ObtPipewire_LIBRARIES} )
    target_link_directories (ork_lev2 PRIVATE $ENV{OBT_VULKAN_ROOT}/lib)

    list(APPEND libs vulkan )
    IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
      list(APPEND libs openvr_api )

      IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
      IF(EXISTS ENV{RENDERDOC_DIR}/include/)
        target_include_directories(ork_lev2 PRIVATE ENV{RENDERDOC_DIR}/include/)
        set(RENDERDOC_API_ENABLED:BOOL=ON)
        target_compile_definitions( ork_lev2 PRIVATE RENDERDOC_API_ENABLED )
      ENDIF()
      ENDIF()

    ENDIF()


ENDIF()

list(APPEND libs  mpfr gmp )
list(APPEND libs  openvdb openvdb_ax tbb )

target_include_directories (ork_lev2 PRIVATE ${INCD} )
target_include_directories (ork_lev2 PRIVATE ${COREINCD} )
target_include_directories (ork_lev2 PRIVATE ${FTXUIINCD} )
target_include_directories (ork_lev2 PRIVATE ${CGAL_INCLUDE_DIR} )
target_include_directories (ork_lev2 PRIVATE $ENV{OBT_STAGE}/include/bullet )


target_link_libraries(ork_lev2 PRIVATE ${libs} )
target_link_libraries(ork_lev2 PRIVATE assimp sndfile shaderc_shared )

target_link_libraries(ork_lev2 LINK_PRIVATE Boost::filesystem )
target_link_libraries(ork_lev2 LINK_PRIVATE ork_ftxui )

ork_torch_opts(ork_lev2) # torch should go last, so it cannot override the other libs

###################################
# intel specific
###################################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
  target_link_libraries(ork_lev2 PUBLIC triangle )
ENDIF()

###################################

IF(${APPLE})
ELSE()
  target_link_options(ork_lev2 PUBLIC -Wl,--no-undefined )
  target_link_libraries(ork_lev2 PUBLIC X11 asound )
ENDIF()
###################################

install(TARGETS ork_lev2 LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )

###################################

add_subdirectory (unittests)
add_subdirectory (integrationtests/singularity)
add_subdirectory (integrationtests/ui)
add_subdirectory (integrationtests/auddev)
add_subdirectory (examples)
add_subdirectory (pyext)
add_subdirectory (utils/shadlang)
add_subdirectory (utils/vdbtest1)
add_subdirectory (utils/vdbviewer)
IF(${APPLE})
  #add_subdirectory (utils/osxhmdenum)
ELSE()
add_subdirectory (utils/luxhmdenum)
ENDIF()
