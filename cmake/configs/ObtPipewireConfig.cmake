
# this gets invoked MULTIPLE times per cmake invocation
# hence we cannot add targets here
# because you will end up with duplicate targets

if(ObtPipewire_FOUND)
endif()

#set_property(TARGET ObtPipewire PROPERTY EXPORT_NAME ObtPipewire)
#set_property(GLOBAL PROPERTY MyConfig_EXPORTS MyConfig.cmake)
#set_property(TARGET ObtOpenBlasLibTarget APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "MY_VAR=${MY_VAR}")
#mark_as_advanced(ObtOpenBlas_INCLUDE_DIRS ObtOpenBlas_LIBRARIES)
#set_property(GLOBAL PROPERTY ObtOpenBlas_INCLUDE_DIRS ${ObtOpenBlas_INCLUDE_DIRS})
#set_property(GLOBAL PROPERTY ObtOpenBlas_LIBRARIES ${ObtOpenBlas_LIBRARIES})
#set_target_properties(ObtOpenBlasLibTarget PROPERTIES EXPORT_NAME ObtOpenBlasLibTarget)