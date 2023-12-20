# FindObtPipewire.cmake
# ------------------
# Try to find Pipewire library
# Once done, this will define
#  ObtPipewire_FOUND - System has Pipewire
#  ObtPipewire_INCLUDE_DIRS - The Pipewire include directories
#  ObtPipewire_LIBRARIES - The libraries needed to use Pipewire

# Guard variable to prevent multiple executions
if(NOT OBT_PIPEWIRE_PRAGMA_ONCE)
    set(OBT_PIPEWIRE_PRAGMA_ONCE TRUE)

    find_path( ObtPipewireSpaInclude_DIR
            NAMES spa/buffer/buffer.h 
            HINTS $ENV{OBT_STAGE}/include
            PATH_SUFFIXES spa spa-0.2
            )

    find_path( ObtPipewireInclude_DIR 
            NAMES pipewire/pipewire.h 
            HINTS $ENV{OBT_STAGE}/include
            PATH_SUFFIXES pipewire pipewire-0.3
            )

    find_library( ObtPipewireLibrary NAMES libpipewire-0.3.so.0 HINTS $ENV{OBT_STAGE}/lib )

    include(FindPackageHandleStandardArgs)

    find_package_handle_standard_args(ObtPipewire REQUIRED_VARS ObtPipewireInclude_DIR ObtPipewireInclude_DIR )


    if(ObtPipewire_FOUND)
    # this gets invoked ONCE per cmake invocation
    set(ObtPipewire_DIR $ENV{ORKID_WORKSPACE_DIR}/cmake/configs)
    set(ObtPipewire_LIBRARIES ${ObtPipewireLibrary} )  
    message( "ObtPipewire_DIR: " ${ObtPipewire_DIR} )
    message( "ObtPipewireSpaInclude_DIR: " ${ObtPipewireSpaInclude_DIR} )
    message( "ObtPipewireInclude_DIR: " ${ObtPipewireInclude_DIR} )
    message( "ObtPipewire_LIBRARIES: " ${ObtPipewire_LIBRARIES} )
    endif()

    mark_as_advanced(ObtPipewire_DIR ObtPipewire_LIBRARIES ObtPipewire_INCLUDE_DIRS )
endif()