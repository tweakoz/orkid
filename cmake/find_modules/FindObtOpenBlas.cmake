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
  set(ObtOpenBlas_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake/configs)
endif()

mark_as_advanced(ObtOpenBlas_DIR)
