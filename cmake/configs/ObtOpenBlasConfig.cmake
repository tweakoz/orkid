
# this gets invoked MULTIPLE times per cmake invocation
# hence we cannot add targets here
# because you will end up with duplicate targets
if(ObtOpenBlas_FOUND)
endif()
