cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

################################################################################

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_SCAN_FOR_MODULES off)

################################################################################
# Set default build type flags
################################################################################

IF(NOT DEFINED IOS_BUILD)
  set(IOS_BUILD OFF)
ENDIF()

################################################################################

set(CMAKE_INSTALL_RPATH "$ENV{OBT_SUBSPACE_LIB_DIR}")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

#############################################################################################################

function(orkid_find_python)

  #################################
  # hints for find_package
  #  to find the python for the active subspace 
  #################################

  set(CMAKE_FIND_DEBUG_MODE OFF)
  set(PYTHON_EXECUTABLE $ENV{OBT_PYTHONHOME}/bin/python3)
  set(PYTHON_LIBRARY $ENV{OBT_PYTHONHOME}/lib/libpython3.12.dylib)
  set(PYTHON_LIBRARY $ENV{OBT_PYTHONHOME}/lib/libpython3.12.so)

  set(Python3_FIND_STRATEGY "LOCATION")
  set(Python3_ROOT_DIR $ENV{OBT_PYTHONHOME} )
  #set(Python3_FIND_VIRTUALENV ONLY)

  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
  #find_package(pybind11 REQUIRED)

  #################################
  # export found python variables
  # to parent scope
  #################################

  #message( ${Python3_INCLUDE_DIRS} )
  set(Python3_INCLUDE_DIRS ${Python3_INCLUDE_DIRS} PARENT_SCOPE)
  set(Python3_LIBRARY_DIRS ${Python3_LIBRARY_DIRS} PARENT_SCOPE)
  set(Python3_RUNTIME_LIBRARY_DIRS ${Python3_RUNTIME_LIBRARY_DIRS} PARENT_SCOPE)
  set(Python3_LINK_OPTIONS ${Python3_LINK_OPTIONS} PARENT_SCOPE)
  set(Python3_LIBRARIES ${Python3_LIBRARIES} PARENT_SCOPE)

  #################################

endfunction()

IF(NOT IOS_BUILD)
  orkid_find_python()
ENDIF()

#message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

IF(NOT IOS_BUILD)
  find_package(ObtOpenBlas REQUIRED)
  IF(APPLE)
  ELSE()
  find_package(ObtPipewire REQUIRED)
  ENDIF()
ENDIF()

################################################################################
# enable python for a given target
#  meant to be used by orkid, and orkid based projects
#  as it will function correctly for the host and other subspaces
################################################################################

function(enable_python_on_target the_target)
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS ${Python3_INCLUDE_DIRS})
  target_include_directories(${the_target} PUBLIC ${Python3_INCLUDE_DIRS} )
  target_link_directories(${the_target} PUBLIC ${Python3_RUNTIME_LIBRARY_DIRS} )
  target_link_directories(${the_target} PUBLIC ${Python3_LIBRARY_DIRS} )
  target_link_options(${the_target} PUBLIC ${Python3_LINK_OPTIONS})
  target_link_libraries(${the_target} PUBLIC ${Python3_LIBRARIES} )
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_PUBLIC_LIBPATHS PUBLIC $ENV{OBT_PYTHON_LIB_PATH}  )
endfunction()

#############################################################################################################

function(enable_memdebug_on_target the_target)
  target_compile_options(${the_target} PUBLIC -fno-omit-frame-pointer)
  target_compile_options(${the_target} PUBLIC -fsanitize=address)
  target_compile_options(${the_target} PUBLIC -fsanitize=undefined)
  target_compile_options(${the_target} PUBLIC -fsanitize=leak)
  target_link_options(${the_target} PUBLIC -fsanitize=address)
endfunction()

#############################################################################################################
# Global sanitizer support (set via -DSANITIZER=ADDRESS|THREAD|UNDEFINED)
#############################################################################################################

IF(DEFINED SANITIZER AND NOT SANITIZER STREQUAL "")
  #message(STATUS "Sanitizer enabled: ${SANITIZER}")

  # Common flags for all sanitizers
  add_compile_options(-fno-omit-frame-pointer)
  add_compile_options(-fno-optimize-sibling-calls)

  IF(SANITIZER STREQUAL "ADDRESS")
    # AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer
    add_compile_options(-fsanitize=address)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=address)
    add_link_options(-fsanitize=undefined)
    #message(STATUS "  -> ASan + UBSan + LeakSan enabled")

  ELSEIF(SANITIZER STREQUAL "THREAD")
    # ThreadSanitizer + UndefinedBehaviorSanitizer
    # Note: TSan cannot be combined with ASan or LeakSan
    add_compile_options(-fsanitize=thread)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=thread)
    add_link_options(-fsanitize=undefined)
    #message(STATUS "  -> TSan + UBSan enabled")

  ELSEIF(SANITIZER STREQUAL "UNDEFINED")
    # UndefinedBehaviorSanitizer only
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
    #message(STATUS "  -> UBSan enabled")

  ELSE()
    message(FATAL_ERROR "Unknown SANITIZER value: ${SANITIZER}. Use ADDRESS, THREAD, or UNDEFINED.")
  ENDIF()
ENDIF()

#############################################################################################################

SET(BUILD_SHARED_LIBS ON)

# Pin Boost discovery to OBT staging — HINTS + NO_DEFAULT_PATH stops
# CMake from auto-finding /opt/homebrew/opt/boost on macOS.
IF(IOS_BUILD)
  # iOS only needs minimal Boost components
  find_package(Boost REQUIRED COMPONENTS system filesystem
               HINTS $ENV{OBT_STAGE}/lib/cmake/Boost-1.82.0
               NO_DEFAULT_PATH)
ELSE()
  # Full build needs all components
  find_package(Boost REQUIRED COMPONENTS system filesystem program_options
               HINTS $ENV{OBT_STAGE}/lib/cmake/Boost-1.82.0
               NO_DEFAULT_PATH)
ENDIF()

#############################################################################################################

set( ORKROOT $ENV{ORKID_WORKSPACE_DIR} )
set( ORK_CORE_INCD ${ORKROOT}/ork.core/inc )
set( ORK_LEV2_INCD ${ORKROOT}/ork.lev2/inc )
set( ORK_ECS_INCD ${ORKROOT}/ork.ecs/inc )
set( ORK_ECS_SRCD ${ORKROOT}/ork.ecs/src )

################################################################################
# iOS Detection
################################################################################

IF(IOS_BUILD)
  IF(NOT IOS_BUILD_MSG_PRINTED)
    message(STATUS "iOS Build Detected")
    set(IOS_BUILD_MSG_PRINTED TRUE CACHE INTERNAL "iOS build message already printed")
  ENDIF()
  set(ENABLE_PYTHON OFF CACHE BOOL "Disable Python for iOS" FORCE)
  set(ENABLE_OPENCL OFF CACHE BOOL "Disable OpenCL for iOS" FORCE)
  set(BUILD_IOS_MINIMAL ON CACHE BOOL "Build minimal iOS library" FORCE)
ENDIF()

################################################################################

# Note: HOMEBREW_PREFIX block removed — orkid no longer pulls includes/libs
# from /opt/homebrew. All previously-brew dependencies (libsodium, xxhash,
# gmp, mpfr, libsndfile, shaderc) are now OBT-built and live under
# $ENV{OBT_STAGE}/{include,lib}, which is already on the include/link paths.

################################################################################

IF(APPLE AND NOT IOS_BUILD)
    # macOS-specific settings (don't override iOS toolchain settings)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.5)
    set(CMAKE_OSX_SYSROOT $ENV{OBT_MACOS_SDK_DIR})
    set(CMAKE_MACOSX_RPATH 1)
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
       SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
    ENDIF("${isSystemDir}" STREQUAL "-1")

    macro(ADD_OSX_FRAMEWORK fwname target)
        find_library(FRAMEWORK_${fwname}
        NAMES ${fwname}
        PATHS ${CMAKE_OSX_SYSROOT}/System/Library
        PATH_SUFFIXES Frameworks
        NO_DEFAULT_PATH)
        if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
            MESSAGE(ERROR ": Framework ${fwname} not found")
        else()
            TARGET_LINK_LIBRARIES(${target} PUBLIC "${FRAMEWORK_${fwname}}/${fwname}")
            MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
        endif()
    endmacro(ADD_OSX_FRAMEWORK)

    ############################################################################
    # ADD_STAGED_FRAMEWORK - Link against a framework installed in staging lib
    #
    # For frameworks installed via obt.macos.install_framework_to_stage(),
    # which have install names using @rpath/xxx.framework/...
    #
    # Usage: ADD_STAGED_FRAMEWORK(xxx my_target)
    ############################################################################
    function(ADD_STAGED_FRAMEWORK fwname target)
        set(FW_BASE "$ENV{OBT_STAGE}/lib/${fwname}.framework")
        set(FW_HEADERS "${FW_BASE}/Headers")
        set(FW_BINARY "${FW_BASE}/${fwname}")

        # Verify framework exists
        if(NOT EXISTS "${FW_BASE}")
            message(FATAL_ERROR "Staged framework not found: ${FW_BASE}")
        endif()

        # Add header search path
        target_include_directories(${target} PRIVATE "${FW_HEADERS}")

        # Link against the framework binary
        target_link_libraries(${target} PRIVATE "${FW_BINARY}")

        # Ensure RPATH includes staging lib for @rpath resolution
        # This should already be set globally, but ensure it for this target
        set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "$ENV{OBT_STAGE}/lib")
        set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "$ENV{OBT_STAGE}/lib")

        message(STATUS "Staged framework ${fwname} added from ${FW_BASE}")
    endfunction()

    set(CMAKE_MACOSX_RPATH 1)
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
       SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
    ENDIF("${isSystemDir}" STREQUAL "-1")
ENDIF()

##############################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    add_compile_options(-march=native)
ELSEIF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
ENDIF()

#############################################################################################################

function(ork_std_target_set_incdirs the_target)

  # obt.pybind11/ before everything else — pybind11 headers live at
  # $OBT_STAGE/include/obt.pybind11/pybind11/ (see pybind11.py). Putting this
  # FIRST guarantees `#include <pybind11/...>` always resolves to OBT's
  # pybind11
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include/obt.pybind11 )
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include/eigen3 )
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include)
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include/tuio/oscpack)

  # IGL (its a beast, needs a cmake update)
  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_BUILDS}/igl/include )
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_BUILDS}/igl/external/triangle )

    IF(APPLE)
    ELSE()
      set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS /usr/include/libdrm )
    ENDIF()


  ENDIF()

  # (Previously appended ${HOMEBREW_PREFIX}/include here on APPLE; removed —
  # $ENV{OBT_STAGE}/include is added unconditionally above.)

  IF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_BUILDS}/sse2neon )
  ENDIF()

endfunction()

#############################################################################################################

function(ork_std_target_set_defs the_target)

  set( def_list "" )

  IF(${BUILDING_ORKID})
    list(APPEND def_list -DBUILDING_ORKID)
    ELSE()
    list(APPEND def_list -DUSING_ORKID)
  ENDIF()


  IF(PROFILER)
    list(APPEND def_list -DORK_PROFILER_ENABLE)
  ENDIF()

#  message(STATUS "ARCHITECTURE: ${ARCHITECTURE}")

  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    list(APPEND def_list -DORK_ARCHITECTURE_X86_64)
  ELSEIF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
    list(APPEND def_list -DORK_ARCHITECTURE_ARM_64)
    list(APPEND def_list -DKLEIN_ARCHITECTURE_ARM)
  ELSE()
    MESSAGE( FATAL_ERROR "unsupported architecture ${ARCHITECTURE}")
  ENDIF()

  if(IOS_BUILD)
    list(APPEND def_list -DORK_IOS -DORK_CONFIG_IOS)
  elseif(${APPLE})
    list(APPEND def_list -DOSX -DORK_OSX )
  ELSE()
    list(APPEND def_list -DORK_CONFIG_IX -DLINUX -DGCC )
    list(APPEND def_list -D_REENTRANT -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE )
  ENDIF()

  list(SORT def_list)

  set_property( TARGET ${the_target} APPEND PROPERTY TGT_DEFINITIIONS ${def_list} )

endfunction()

#############################################################################################################

function(ork_std_target_set_opts the_target)

  set( opt_list "" )

  list(APPEND opt_list -Wall -Wpedantic)
  list(APPEND opt_list -Wno-deprecated -Wno-register -Wno-switch-enum)
  list(APPEND opt_list -Wno-unused-command-line-argument)
  list(APPEND opt_list -Wno-unused -Wno-extra-semi -Wno-c99-designator)
  list(APPEND opt_list -fPIE -fPIC )
  list(APPEND opt_list -fextended-identifiers)
  list(APPEND opt_list -fexceptions)
  list(APPEND opt_list -fvisibility=default)
  list(APPEND opt_list -fno-common -fno-strict-aliasing )
  list(APPEND opt_list -g  )


  IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND opt_list -frounding-math) # CGAL!
  ENDIF()

  #list(SORT def_list)

  set_property( TARGET ${the_target} APPEND PROPERTY TGT_OPTIONS ${opt_list} )

endfunction()

#############################################################################################################

function(ork_std_target_opts_compiler the_target)

  ork_std_target_set_opts(${the_target})
  ork_std_target_set_defs(${the_target})
  ork_std_target_set_incdirs(${the_target})

  ################################################################################
  # standardized header search paths
  ################################################################################

  get_property( TGT_INCLUDE_PATHS TARGET ${the_target} PROPERTY TGT_INCLUDE_PATHS )
  target_include_directories(${the_target} PRIVATE ${TGT_INCLUDE_PATHS} )

  ################################################################################
  # standardized compile options
  ################################################################################

  get_property( TGT_OPTIONS TARGET ${the_target} PROPERTY TGT_OPTIONS )
  target_compile_options(${the_target} PRIVATE ${TGT_OPTIONS})

  ################################################################################
  # standardized definitions
  ################################################################################

  get_property( TGT_DEFINITIIONS TARGET ${the_target} PROPERTY TGT_DEFINITIIONS )
  target_compile_definitions(${the_target} PRIVATE ${TGT_DEFINITIIONS} )

endfunction()

#############################################################################################################

function(ork_core_target_opts_compiler the_target)
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.core/inc )
endfunction()

function(ork_core_target_opts_linker the_target)
  target_link_libraries(${the_target} LINK_PRIVATE ork_core )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.core/inc )
  target_link_libraries(${the_target} LINK_PRIVATE easy_profiler )
endfunction()

function(ork_std_target_opts_core the_target)
  ork_core_target_opts_compiler(${the_target})
  ork_core_target_opts_linker(${the_target})
endfunction()

#############################################################################################################

function(ork_lev2_target_opts_compiler the_target)
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.lev2/inc )
  # suppress pytorch warnings
  #target_compile_options(${the_target} PRIVATE -Wno-gnu-zero-variadic-macro-arguments )
  endfunction()

function(ork_lev2_target_opts_linker the_target)
  target_link_libraries(${the_target} LINK_PRIVATE ork_lev2 )
  target_link_libraries(${the_target} LINK_PRIVATE Boost::system )
  set_target_properties(${the_target} PROPERTIES LINKER_LANGUAGE CXX)
  IF(APPLE)
    set_target_properties(${the_target} PROPERTIES
      INSTALL_RPATH "$ENV{OBT_STAGE}/lib;"
      BUILD_WITH_INSTALL_RPATH TRUE
    )
  ELSE()
    set_target_properties(${the_target} PROPERTIES
      INSTALL_RPATH $ENV{OBT_STAGE}/lib:
      BUILD_WITH_INSTALL_RPATH TRUE
    )
  ENDIF()
    endfunction()

function(ork_std_target_opts_lev2 the_target)
  ork_lev2_target_opts_compiler(${the_target})
  ork_lev2_target_opts_linker(${the_target})
endfunction()

#############################################################################################################

function(ork_ecs_target_opts_compiler the_target)
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.ecs/inc )
  target_include_directories (${the_target} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
  endfunction()

function(ork_ecs_target_opts_linker the_target)
  target_link_libraries(${the_target} LINK_PRIVATE ork_ecs )
  target_link_libraries(${the_target} LINK_PRIVATE Boost::system )
  set_target_properties(${the_target} PROPERTIES LINKER_LANGUAGE CXX)
  # NOTE: INSTALL_RPATH REPLACES (does not append)
  # Keep the full list, same form as the lev2 opts.
  IF(APPLE)
    set_target_properties(${the_target} PROPERTIES
      INSTALL_RPATH "$ENV{OBT_STAGE}/lib;"
      BUILD_WITH_INSTALL_RPATH TRUE
    )
  ELSE()
    set_target_properties(${the_target} PROPERTIES
      INSTALL_RPATH $ENV{OBT_STAGE}/lib:
      BUILD_WITH_INSTALL_RPATH TRUE
    )
  ENDIF()
endfunction()

function(ork_std_target_opts_ecs the_target)
  ork_ecs_target_opts_compiler(${the_target})
  ork_ecs_target_opts_linker(${the_target})
endfunction()

#############################################################################################################

function(ork_utpp_target_opts_compiler the_target)
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.utpp/inc )
endfunction()

function(ork_utpp_target_opts_linker the_target)
  target_link_libraries(${the_target} LINK_PRIVATE ork_utpp )
endfunction()

function(ork_std_target_opts_utpp the_target)
  ork_utpp_target_opts_compiler(${the_target})
  ork_utpp_target_opts_linker(${the_target})
endfunction()

#############################################################################################################

function( ork_ecs_target_opts_compiler the_target)
  ork_lev2_target_opts_compiler(${the_target})
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.core/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.lev2/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.ecs/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.ecs/src )
  target_include_directories (${the_target} PRIVATE ${SRCD} )
  target_include_directories (${the_target} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
endfunction()

#############################################################################################################

function(setupCoreEXE target sources)
  add_executable (${target} ${sources} ${ARGN} )
  ork_std_target_opts_exe(${target})
  ork_std_target_opts_core(${target})
endfunction()

#############################################################################################################

function(setupLev2COM target)
  ork_std_target_opts_exe(${target})
  ork_std_target_opts_core(${target})
  ork_std_target_opts_lev2(${target})
endfunction()

function(setupLev2EXE target sources)
  add_executable (${target} ${sources} ${ARGN} )
  setupLev2COM(${target})
endfunction()

function(setupLev2SHLIB target sources)
  add_library (${target} SHARED ${sources} ${ARGN} )
  setupLev2COM(${target})
endfunction()

#############################################################################################################

function(setupEcsEXE target sources)
  add_executable (${target} ${sources} ${ARGN} )
  ork_std_target_opts_exe(${target})
  ork_std_target_opts_core(${target})
  ork_std_target_opts_lev2(${target})
  ork_std_target_opts_ecs(${target})
endfunction()

#############################################################################################################

function(ork_std_target_set_libdirs the_target)

  list(APPEND private_libdir_list "$ENV{OBT_SUBSPACE_LIB_DIR}" )
  list(APPEND private_libdir_list "$ENV{OBT_STAGE}/lib" )

  ################################################################################
  # IGL (its a beast, needs a cmake update)
  ################################################################################

  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )

    list(APPEND CMAKE_MODULE_PATH "$ENV{OBT_STAGE}/lib/cmake/igl" )
    set( LIBIGL_DIR $ENV{OBT_STAGE}/lib/cmake/igl )

    option(LIBIGL_USE_STATIC_LIBRARY "Use libigl as static library" OFF)
    #option(LIBIGL_WITH_ANTTWEAKBAR      "Use AntTweakBar"    OFF)

    option(LIBIGL_WITH_CGAL             "Use CGAL"           OFF)
    option(LIBIGL_WITH_COMISO           "Use CoMiso"         ON)
    option(LIBIGL_WITH_CORK             "Use Cork"           ON)
    option(LIBIGL_WITH_EMBREE           "Use Embree"         ON)
    #option(LIBIGL_WITH_LIM              "Use LIM"            OFF)
    #option(LIBIGL_WITH_MATLAB           "Use Matlab"         OFF)
    #option(LIBIGL_WITH_MOSEK            "Use MOSEK"          OFF)
    #option(LIBIGL_WITH_OPENGL           "Use OpenGL"         ON)
    #option(LIBIGL_WITH_OPENGL_GLFW      "Use GLFW"           ON)
    #option(LIBIGL_WITH_PNG              "Use PNG"            OFF)
    #option(LIBIGL_WITH_PYTHON           "Use Python"         OFF)
    option(LIBIGL_WITH_TETGEN           "Use Tetgen"         OFF)
    option(LIBIGL_WITH_TRIANGLE         "Use Triangle"       ON)
    #option(LIBIGL_WITH_VIEWER           "Use OpenGL viewer"  ON)
    #option(LIBIGL_WITH_XML              "Use XML"            OFF)
    find_package(LIBIGL REQUIRED)
    #include($ENV{OBT_BUILDS}/igl/cmake/libigl.cmake )

    list(APPEND private_libdir_list "$ENV{OBT_BUILDS}/igl/.build" )

  ENDIF()

  set_target_properties( ${the_target} PROPERTIES TGT_PRIVATE_LIBPATHS "${private_libdir_list}" )

endfunction()

#############################################################################################################

function(ork_std_target_opts_compiler_module the_target )
  ork_std_target_set_opts(${the_target})
  ork_std_target_set_defs(${the_target})
  ork_std_target_set_incdirs(${the_target})
  ork_std_target_set_libdirs(${the_target})
endfunction()

#############################################################################################################

function(ork_std_target_opts_linker the_target)
  ork_std_target_set_libdirs(${the_target})
  get_property( TGT_PRIVATE_LIBPATHS TARGET ${the_target} PROPERTY TGT_PRIVATE_LIBPATHS )
  get_property( TGT_PUBLIC_LIBPATHS TARGET ${the_target} PROPERTY TGT_PUBLIC_LIBPATHS )

  target_link_directories(${the_target} PRIVATE ${TGT_PRIVATE_LIBPATHS} )
  target_link_directories(${the_target} PUBLIC ${TGT_PUBLIC_LIBPATHS} )

  set( BOOST_LIBS "" )
  list(APPEND BOOST_LIBS ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY}  )

  IF(IOS_BUILD)
    # iOS-specific linking (no AppKit, use Foundation instead)
    target_link_libraries(${the_target} LINK_PRIVATE m pthread )
    target_link_libraries(${the_target} LINK_PRIVATE
          "-framework Foundation"
          "-framework Accelerate"
    )
    target_link_libraries(${the_target} LINK_PRIVATE objc ${BOOST_LIBS} )
  ELSEIF(APPLE)
    # (Removed: target_link_directories ${HOMEBREW_PREFIX}/lib — OBT staging
    # lib dir is already added via private_libdir_list / CMAKE_INSTALL_RPATH.)
    target_link_libraries(${the_target} LINK_PRIVATE m pthread )
    target_link_libraries(${the_target} LINK_PRIVATE
          "-framework AppKit"
          "-framework IOKit"
          "-framework Accelerate"
    )
    target_link_libraries(${the_target} LINK_PRIVATE objc ${BOOST_LIBS} )
  ELSEIF(UNIX)
    target_link_libraries(${the_target} LINK_PRIVATE rt dl pthread ${BOOST_LIBS} )
  ENDIF()

  target_link_libraries(${the_target} LINK_PUBLIC ${ObtOpenBlas_LIBRARIES} )

  target_link_libraries(${the_target} LINK_PUBLIC easy_profiler )

  
  endfunction()

#############################################################################################################

function(ork_std_target_opts the_target)
  ork_std_target_opts_compiler(${the_target})
  ork_std_target_opts_linker(${the_target})
endfunction()

function(ork_std_target_opts_exe the_target)
  ork_std_target_opts(${the_target})
  install(TARGETS ${the_target} DESTINATION $ENV{OBT_SUBSPACE_BIN_DIR} )
endfunction()


#############################################################################################################
# ISPC compile option
#############################################################################################################

function(declare_ispc_source_object src obj dep)
  add_custom_command(OUTPUT ${obj}
                     MAIN_DEPENDENCY ${src}
                     COMMENT "ISPC-Compile ${src}"
                     COMMAND ispc -O3 --target=avx ${src} -g -o ${obj} --colored-output
                     DEPENDS ${dep})
endfunction()

#############################################################################################################
# ISPC convenience method
#  I really dislike cmake as a language...
#############################################################################################################

function(gen_ispc_object_list
         ISPC_GLOB_SPEC
         ISPC_SUBDIR
         ISPC_OUTPUT_OBJECT_LIST )
  set(ISPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/${ISPC_SUBDIR})
  #message(${ISPC_GLOB_SPEC})
  #message(${ISPC_SUBDIR})
  #message(${ISPC_OUTPUT_DIR})
  file(GLOB_RECURSE SRC_ISPC ${ISPC_GLOB_SPEC} )
  foreach(SRC_ITEM ${SRC_ISPC})
    file(RELATIVE_PATH SRC_ITEM_RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SRC_ITEM} )
    get_filename_component(ispc_name_we ${SRC_ITEM_RELATIVE} NAME_WE)
    get_filename_component(ispc_dir ${SRC_ITEM_RELATIVE} DIRECTORY)
    set(OBJ_ITEM ${ISPC_OUTPUT_DIR}/${ispc_name_we}.o)
    #message(${SRC_ITEM})
    #message(${ispc_name_we})
    #message(${ispc_dir})
    #message(${OBJ_ITEM})
    declare_ispc_source_object(${SRC_ITEM} ${OBJ_ITEM} ${SRC_ITEM} )
    list(APPEND _INTERNAL_ISPC_OUTPUT_OBJECT_LIST ${OBJ_ITEM} )
  endforeach(SRC_ITEM)
  #message(${_INTERNAL_ISPC_OUTPUT_OBJECT_LIST})
  set(${ISPC_OUTPUT_OBJECT_LIST} ${_INTERNAL_ISPC_OUTPUT_OBJECT_LIST} PARENT_SCOPE)
endfunction()

#############################################################################################################
# Swift Module Build Helper
#############################################################################################################

function(ork_build_swift_module)
  # Parse arguments
  set(options "")
  set(oneValueArgs NAME MODULE_DIR BRIDGE_HEADER INCLUDE_DIR)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(SWIFT_MOD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Validate required arguments
  if(NOT SWIFT_MOD_NAME)
    message(FATAL_ERROR "ork_build_swift_module: NAME argument is required")
  endif()
  if(NOT SWIFT_MOD_MODULE_DIR)
    message(FATAL_ERROR "ork_build_swift_module: MODULE_DIR argument is required")
  endif()
  if(NOT SWIFT_MOD_SOURCES)
    message(FATAL_ERROR "ork_build_swift_module: SOURCES argument is required")
  endif()
  if(NOT SWIFT_MOD_BRIDGE_HEADER)
    message(FATAL_ERROR "ork_build_swift_module: BRIDGE_HEADER argument is required")
  endif()
  if(NOT SWIFT_MOD_INCLUDE_DIR)
    message(FATAL_ERROR "ork_build_swift_module: INCLUDE_DIR argument is required")
  endif()

  # Set paths
  set(SWIFT_MODULE_OUTPUT_DIR ${CMAKE_INSTALL_PREFIX}/lib/swift)
  set(SWIFT_BUILD_DIR ${CMAKE_BINARY_DIR}/swift_modules)
  file(MAKE_DIRECTORY ${SWIFT_BUILD_DIR})

  set(SWIFT_MODULE_FILE ${SWIFT_BUILD_DIR}/${SWIFT_MOD_NAME}.swiftmodule)
  set(SWIFT_MODULE_DOC ${SWIFT_BUILD_DIR}/${SWIFT_MOD_NAME}.swiftdoc)
  set(SWIFT_MODULE_DYLIB ${SWIFT_BUILD_DIR}/lib${SWIFT_MOD_NAME}.dylib)

  message(STATUS "Swift module '${SWIFT_MOD_NAME}': ${SWIFT_BUILD_DIR}")

  # Compile Swift module
  add_custom_command(
    OUTPUT ${SWIFT_MODULE_FILE} ${SWIFT_MODULE_DOC} ${SWIFT_MODULE_DYLIB}
    COMMAND ${SWIFTC}
      ${SWIFT_MOD_SOURCES}
      -module-name ${SWIFT_MOD_NAME}
      -emit-module
      -emit-module-path ${SWIFT_MODULE_FILE}
      -emit-library
      -o ${SWIFT_MODULE_DYLIB}
      -import-objc-header ${SWIFT_MOD_BRIDGE_HEADER}
      -I ${SWIFT_MOD_INCLUDE_DIR}
      -Xcc -I${SWIFT_MOD_INCLUDE_DIR}
      $<TARGET_FILE:ork_core>
      -Xlinker -rpath -Xlinker ${CMAKE_INSTALL_PREFIX}/lib
      -Xlinker -install_name -Xlinker @rpath/lib${SWIFT_MOD_NAME}.dylib
    DEPENDS ${SWIFT_MOD_SOURCES} ${SWIFT_MOD_BRIDGE_HEADER} ork_core
    COMMENT "Building Swift module: ${SWIFT_MOD_NAME}"
    VERBATIM
  )

  # Create custom target
  add_custom_target(swift_module_${SWIFT_MOD_NAME} ALL
    DEPENDS ${SWIFT_MODULE_FILE} ${SWIFT_MODULE_DYLIB}
  )

  # Install module files to lib/swift/ and dylib to lib/
  install(FILES ${SWIFT_MODULE_FILE} ${SWIFT_MODULE_DOC}
          DESTINATION ${SWIFT_MODULE_OUTPUT_DIR})
  install(FILES ${SWIFT_MODULE_DYLIB}
          DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

endfunction()

#############################################################################################################
# Swift Test Build Helper
#############################################################################################################

function(ork_add_swift_test)
  # Parse arguments
  set(options "")
  set(oneValueArgs NAME SOURCE_DIR MAIN_SOURCE BRIDGE_HEADER INCLUDE_DIR)
  set(multiValueArgs "")
  cmake_parse_arguments(SWIFT_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Validate required arguments
  if(NOT SWIFT_TEST_NAME)
    message(FATAL_ERROR "ork_add_swift_test: NAME argument is required")
  endif()
  if(NOT SWIFT_TEST_SOURCE_DIR)
    message(FATAL_ERROR "ork_add_swift_test: SOURCE_DIR argument is required")
  endif()
  if(NOT SWIFT_TEST_MAIN_SOURCE)
    message(FATAL_ERROR "ork_add_swift_test: MAIN_SOURCE argument is required")
  endif()
  if(NOT SWIFT_TEST_BRIDGE_HEADER)
    message(FATAL_ERROR "ork_add_swift_test: BRIDGE_HEADER argument is required")
  endif()
  if(NOT SWIFT_TEST_INCLUDE_DIR)
    message(FATAL_ERROR "ork_add_swift_test: INCLUDE_DIR argument is required")
  endif()

  # Set paths - use absolute paths to avoid CMake variable expansion issues
  set(SWIFT_OUTPUT_DIR ${CMAKE_INSTALL_PREFIX}/bin)
  set(SWIFT_BUILD_DIR ${CMAKE_BINARY_DIR}/swift_build)
  set(SWIFT_MODULE_DIR ${CMAKE_BINARY_DIR}/swift_modules)
  file(MAKE_DIRECTORY ${SWIFT_BUILD_DIR})
  set(SWIFT_OBJ_FILE ${SWIFT_BUILD_DIR}/${SWIFT_TEST_NAME}.o)
  set(SWIFT_EXECUTABLE ${SWIFT_OUTPUT_DIR}/ork.test.swift.core.${SWIFT_TEST_NAME}.exe)
  set(SWIFT_SOURCE ${SWIFT_TEST_SOURCE_DIR}/${SWIFT_TEST_MAIN_SOURCE})

  # Create output directory
  file(MAKE_DIRECTORY ${SWIFT_OUTPUT_DIR})

  message(STATUS "Swift test '${SWIFT_TEST_NAME}': ${SWIFT_EXECUTABLE}")

  # Compile Swift source to object file
  add_custom_command(
    OUTPUT ${SWIFT_OBJ_FILE}
    COMMAND ${SWIFTC}
      -c ${SWIFT_SOURCE}
      -o ${SWIFT_OBJ_FILE}
      -I ${SWIFT_MODULE_DIR}
      -import-objc-header ${SWIFT_TEST_BRIDGE_HEADER}
      -I ${SWIFT_TEST_INCLUDE_DIR}
      -Xcc -I${SWIFT_TEST_INCLUDE_DIR}
    DEPENDS ${SWIFT_SOURCE} ${SWIFT_TEST_BRIDGE_HEADER} ork_core
            ${SWIFT_MODULE_DIR}/OrkCore.swiftmodule
            ${SWIFT_MODULE_DIR}/libOrkCore.dylib
    COMMENT "Compiling Swift test: ${SWIFT_TEST_NAME}"
    VERBATIM
  )

  # Link Swift executable
  add_custom_command(
    OUTPUT ${SWIFT_EXECUTABLE}
    COMMAND ${SWIFTC}
      ${SWIFT_OBJ_FILE}
      -o ${SWIFT_EXECUTABLE}
      -I ${SWIFT_MODULE_DIR}
      -L ${SWIFT_MODULE_DIR}
      $<TARGET_FILE:ork_core>
      ${SWIFT_MODULE_DIR}/libOrkCore.dylib
      -Xlinker -rpath -Xlinker ${CMAKE_INSTALL_PREFIX}/lib
      -Xlinker -rpath -Xlinker ${SWIFT_MODULE_DIR}
    DEPENDS ${SWIFT_OBJ_FILE} ork_core
            ${SWIFT_MODULE_DIR}/libOrkCore.dylib
    COMMENT "Linking Swift test: ${SWIFT_TEST_NAME}"
    VERBATIM
  )

  # Create custom target
  add_custom_target(ork.test.swift.${SWIFT_TEST_NAME} ALL
    DEPENDS ${SWIFT_EXECUTABLE}
  )

  # Install to bin directory
  install(PROGRAMS ${SWIFT_EXECUTABLE}
          DESTINATION bin)

endfunction()

