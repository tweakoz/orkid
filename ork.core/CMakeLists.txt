project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )

###################################

file(GLOB_RECURSE src_application ${SRCD}/application/*.cpp)
file(GLOB_RECURSE src_asset ${SRCD}/asset/*.cpp)
file(GLOB_RECURSE src_dataflow ${SRCD}/dataflow/*.cpp)
file(GLOB_RECURSE src_event ${SRCD}/event/*.cpp)
file(GLOB_RECURSE src_file ${SRCD}/file/*.cpp)
file(GLOB_RECURSE src_kernel ${SRCD}/kernel/*.cpp)
file(GLOB_RECURSE src_math ${SRCD}/math/*.cpp)
file(GLOB_RECURSE src_object ${SRCD}/object/*.cpp)
file(GLOB_RECURSE src_reflect ${SRCD}/reflect/*.cpp)
file(GLOB_RECURSE src_rtti ${SRCD}/rtti/*.cpp)
file(GLOB_RECURSE src_stream ${SRCD}/stream/*.cpp)
file(GLOB_RECURSE src_util ${SRCD}/util/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_hdl ${SRCD}/hdl/*.cpp)
file(GLOB_RECURSE src_opencl ${SRCD}/opencl/*.cpp)
file(GLOB_RECURSE src_python ${SRCD}/python/*.cpp)
file(GLOB_RECURSE src_test ${SRCD}/test/*.cpp)

set( orksrcs ${src_application} ${src_asset} ${src_dataflow} )
list( APPEND orksrcs ${src_event} ${src_file} ${src_kernel} )
list( APPEND orksrcs ${src_math} ${src_object} ${src_reflect} )
list( APPEND orksrcs ${src_rtti} ${src_stream} ${src_util} )
list( APPEND orksrcs ${src_gfx} ${src_hdl} ${src_opencl} ${src_python} ${src_test} )

###################################
# ISPC files (intel specific)
###################################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )

    gen_ispc_object_list( ${SRCD}/math/*.ispc
                          ork_core.dir/src/math/
                          ispc_math_object_list )

    list( APPEND orksrcs ${ispc_math_object_list} )

ENDIF()

###################################
# Regular (non-iOS) Build Target
###################################

IF(NOT BUILD_IOS_MINIMAL)

  if(NOT TARGET ObtOpenBlasLibTarget)
  #error()
  endif(NOT TARGET ObtOpenBlasLibTarget )

  add_library (ork_core SHARED ${orksrcs} )

ork_std_target_opts(ork_core)
enable_python_on_target(ork_core)
target_link_libraries(ork_core LINK_PRIVATE ork_utpp zmq curlpp curl sodium lz4 tar )

IF(${APPLE})
target_link_libraries(ork_core LINK_PRIVATE "-framework OpenCL" )
ELSE()
target_link_libraries(ork_core LINK_PRIVATE OpenCL )
ENDIF()

target_link_libraries(ork_core LINK_PRIVATE Boost::filesystem xxhash )
target_link_libraries(ork_core LINK_PUBLIC ${ObtOpenBlas_LIBRARIES} )

  find_package(Python COMPONENTS Interpreter Development REQUIRED)
  target_include_directories (ork_core PRIVATE ${INCD} )

  install(TARGETS ork_core LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )

ENDIF()

###################################
# iOS Minimal Build Target
###################################

IF(BUILD_IOS_MINIMAL)

  message(STATUS "Configuring minimal iOS build for ork_core_ios")

  # Math files for iOS
  file(GLOB src_math_ios
    ${SRCD}/math/cvector2_imp.cpp
    ${SRCD}/math/cvector3_imp.cpp
    ${SRCD}/math/cvector4_imp.cpp
    ${SRCD}/math/cmatrix3_imp.cpp
    ${SRCD}/math/cmatrix4_imp.cpp
    ${SRCD}/math/quaternion_imp.cpp
    ${SRCD}/math/misc_math.cpp
    ${SRCD}/math/plane_imp.cpp
    ${SRCD}/math/box.cpp
    ${SRCD}/math/frustum.cpp
    ${SRCD}/math/collision_test.cpp
    ${SRCD}/math/TransformNode.cpp
    ${SRCD}/math/gradient.cpp
    ${SRCD}/math/MultiCurve.cpp
  )

  # Kernel files for iOS
  file(GLOB_RECURSE src_kernel_ios
    ${SRCD}/kernel/opq.cpp
    ${SRCD}/kernel/string/*.cpp
    ${SRCD}/kernel/memcpy.cpp
    ${SRCD}/kernel/prop.cpp
    ${SRCD}/kernel/proptype.cpp
    ${SRCD}/kernel/slashnode.cpp
    ${SRCD}/kernel/datablock.cpp
    ${SRCD}/kernel/datacache.cpp
    ${SRCD}/kernel/environment.cpp
    ${SRCD}/kernel/debug.cpp
    ${SRCD}/kernel/tempstring.cpp
    ${SRCD}/kernel/timer.cpp
    ${SRCD}/kernel/future.cpp
    ${SRCD}/kernel/oldschool.cpp
    ${SRCD}/kernel/semaphore.cpp
    ${SRCD}/kernel/thread.cpp
    ${SRCD}/kernel/error.cpp
  )

  file(GLOB src_file_ios
    ${SRCD}/file/path.cpp
    ${SRCD}/file/file.cpp
    ${SRCD}/file/fileenv.cpp
    ${SRCD}/file/filedev.cpp
    ${SRCD}/file/filedevram.cpp
    ${SRCD}/file/filestd.cpp
  )

  file(GLOB src_object_ios
    ${SRCD}/object/connect.cpp
    ${SRCD}/object/Object.cpp
    ${SRCD}/object/ObjectClass.cpp
    ${SRCD}/object/COM.cpp
  )

  file(GLOB src_rtti_ios
    ${SRCD}/rtti/Category.cpp
    ${SRCD}/rtti/Class.cpp
    ${SRCD}/rtti/rtti.cpp
  )

  file(GLOB_RECURSE src_reflect_ios
    ${SRCD}/reflect/*.cpp
  )

  file(GLOB_RECURSE src_util_ios
    ${SRCD}/util/crc.cpp
    ${SRCD}/util/crc64.cpp
    ${SRCD}/util/md5.cpp
    ${SRCD}/util/hexdump.cpp
    ${SRCD}/util/xxhash.cpp
    ${SRCD}/util/logger.cpp
    ${SRCD}/util/logger_std.cpp
    ${SRCD}/util/logger_redirector.cpp
    ${SRCD}/util/endian.cpp
    ${SRCD}/util/hotkey.cpp
    ${SRCD}/util/URL.cpp
    ${SRCD}/util/tar.cpp
    ${SRCD}/util/download_manager_ios.cpp
    ${SRCD}/util/download.cpp
    ${SRCD}/util/download_group.cpp
    ${SRCD}/util/upload_manager.cpp
    ${SRCD}/util/upload.cpp
    ${SRCD}/util/upload_https_ios.cpp
    ${SRCD}/util/password_provider.cpp
    ${SRCD}/util/dependency/Provider.cpp
    ${SRCD}/util/dependency/Dependent.cpp
    ${SRCD}/util/RingLink.cpp
  )

  file(GLOB src_application_ios
    ${SRCD}/application/application.cpp
  )

  file(GLOB src_asset_ios
    ${SRCD}/asset/Asset.cpp
    ${SRCD}/asset/AssetLoader.cpp
    ${SRCD}/asset/AssetSet.cpp
    ${SRCD}/asset/AssetSetEntry.cpp
    ${SRCD}/asset/NetAssetLoader.cpp
    ${SRCD}/asset/catalog/catalog.cpp
    ${SRCD}/asset/catalog/catalog_impl.cpp
    ${SRCD}/asset/catalog/catalog_impl_cache.cpp
    ${SRCD}/asset/catalog/catalog_impl_get.cpp
    ${SRCD}/asset/catalog/catalog_crypt_ios.cpp
    ${SRCD}/asset/catalog/catalog_dl.cpp
    ${SRCD}/asset/catalog/catalog_manifests.cpp
    ${SRCD}/asset/catalog/catalog_ns.cpp
    ${SRCD}/asset/catalog/uploader.cpp
    ${SRCD}/asset/catalog/chunk_assembler.cpp
    ${SRCD}/asset/catalog/chunk_manifest.cpp
    ${SRCD}/asset/catalog/config.cpp
    ${SRCD}/asset/catalog/configspace.cpp
    ${SRCD}/asset/catalog/entry.cpp
    ${SRCD}/asset/catalog/manifest.cpp
    ${SRCD}/asset/catalog/namespace.cpp
    ${SRCD}/asset/catalog/request.cpp
  )

  file(GLOB src_dataflow_ios
    ${SRCD}/dataflow/dataflow_context.cpp
    ${SRCD}/dataflow/dataflow_sorter.cpp
    ${SRCD}/dataflow/graph_data.cpp
    ${SRCD}/dataflow/graph_inst.cpp
    ${SRCD}/dataflow/module_data.cpp
    ${SRCD}/dataflow/module_inst.cpp
    ${SRCD}/dataflow/plug_data.cpp
    ${SRCD}/dataflow/plug_inst.cpp
    ${SRCD}/dataflow/scheduler.cpp
  )

  set(orksrcs_ios ${src_math_ios} ${src_kernel_ios} ${src_file_ios} ${src_object_ios} ${src_rtti_ios} ${src_reflect_ios} ${src_util_ios} ${src_application_ios} ${src_asset_ios} ${src_dataflow_ios})

  # Create shared library for iOS
  add_library(ork_core_ios SHARED ${orksrcs_ios})

  # iOS-specific compiler options
  target_compile_options(ork_core_ios PRIVATE
    -Wall -Wpedantic
    -Wno-deprecated -Wno-register
    -fPIE -fPIC
    -fexceptions
    -fvisibility=default
  )

  # iOS-specific definitions
  target_compile_definitions(ork_core_ios PRIVATE
    -DORK_IOS
    -DORK_CONFIG_IOS
    -DORK_ARCHITECTURE_ARM_64
    -DKLEIN_ARCHITECTURE_ARM
    -DUSING_ORKID
  )

  # Include directories
  target_include_directories(ork_core_ios PRIVATE ${INCD})
  target_include_directories(ork_core_ios PRIVATE $ENV{OBT_STAGE}/include)

  # iOS is ARM64, use sse2neon to translate SSE to NEON (like macOS Apple Silicon)
  target_include_directories(ork_core_ios PRIVATE $ENV{OBT_BUILDS}/sse2neon)

  # iOS frameworks and libraries (minimal dependencies)
  # Use -iframework to specify iOS SDK framework path
  target_link_options(ork_core_ios PRIVATE
    "-F${CMAKE_OSX_SYSROOT}/System/Library/Frameworks"
  )
  target_link_libraries(ork_core_ios LINK_PRIVATE
    "-framework Foundation"
    "-framework Accelerate"
  )

  # Only link Boost filesystem if available
  find_package(Boost COMPONENTS filesystem system)
  if(Boost_FOUND)
    target_link_libraries(ork_core_ios LINK_PRIVATE Boost::filesystem Boost::system)
  endif()

  # Install iOS library
  install(TARGETS ork_core_ios
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
  )

  # Install headers for iOS SDK distribution
  install(DIRECTORY ${INCD}/ork/
    DESTINATION include/ork
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "*.inl"
    # Exclude Python, OpenCL, and other heavy dependencies
    PATTERN "python" EXCLUDE
    PATTERN "opencl" EXCLUDE
    PATTERN "lev2" EXCLUDE
  )

  message(STATUS "iOS target ork_core_ios configured")

ENDIF()

###################################

IF(NOT IOS_BUILD)
  add_subdirectory (pyext)
  add_subdirectory (examples)
  add_subdirectory (tests)
  add_subdirectory (utils)
  add_subdirectory (tools)
ENDIF()
