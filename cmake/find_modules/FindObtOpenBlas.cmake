# FindOpenBLAS.cmake
# ------------------
# Try to find OpenBLAS library
# Once done, this will define
#  ObtOpenBlas_FOUND - System has OpenBLAS
#  ObtOpenBlas_INCLUDE_DIRS - The OpenBLAS include directories
#  ObtOpenBlas_LIBRARIES - The libraries needed to use OpenBLAS

find_path(ObtOpenBlas_INCLUDE_DIR NAMES cblas.h PATHS $ENV{OBT_STAGE}/include/openblas NO_DEFAULT_PATH)
find_library(ObtOpenBlas_LIBRARY NAMES openblas PATHS $ENV{OBT_STAGE}/lib NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ObtOpenBlas DEFAULT_MSG ObtOpenBlas_INCLUDE_DIR ObtOpenBlas_LIBRARY)

if(ObtOpenBlas_FOUND)
  # this gets invoked ONCE per cmake invocation
  set(ObtOpenBlas_DIR $ENV{ORKID_WORKSPACE_DIR}/cmake/configs)
  set(ObtOpenBlas_LIBRARIES ${ObtOpenBlas_LIBRARY} )  
  message( "ObtOpenBlas_DIR: " ${ObtOpenBlas_DIR} )
  message( "ObtOpenBlas_INCLUDE_DIR: " ${ObtOpenBlas_INCLUDE_DIR} )
  message( "ObtOpenBlas_LIBRARY: " ${ObtOpenBlas_LIBRARY} )
endif()

mark_as_advanced(ObtOpenBlas_DIR ObtOpenBlas_LIBRARY ObtOpenBlas_INCLUDE_DIR ObtOpenBlas_LIBRARIES )
