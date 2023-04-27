
if(ObtOpenBlas_FOUND)
  set(ObtOpenBlas_INCLUDE_DIRS ${OPENBLAS_INCLUDE_DIR})
  set(ObtOpenBlas_LIBRARIES ${OPENBLAS_LIBRARY})
endif()

mark_as_advanced(ObtOpenBlas_INCLUDE_DIR ObtOpenBlas_LIBRARY)
