# FindPipewire.cmake
# ------------------
# Try to find Pipewire library
# Once done, this will define
#  ObtPipewire_FOUND - System has Pipewire
#  ObtPipewire_INCLUDE_DIRS - The Pipewire include directories
#  ObtPipewire_LIBRARIES - The libraries needed to use Pipewire

find_path( ObtPipewireIncludePath
           NAMES spa/buffer/buffer.h 
           HINTS "/usr/include" 
           PATH_SUFFIXES spa spa-0.2
         )

find_path( ObtPipewireSpaIncludePath 
           NAMES pipewire/pipewire.h 
           HINTS "/usr/include" 
           PATH_SUFFIXES pipewire pipewire-0.3
         )

find_library( ObtPipewireLibrary NAMES libpipewire-0.3.so.0 HINTS "/usr/lib/x86_64-linux-gnu/" )
find_library( ObtPipewireSpaLibrary NAMES libpipewire-0.3.so.0 HINTS "/usr/lib/x86_64-linux-gnu/" )


if(ObtOpenBlas_FOUND)
  # this gets invoked ONCE per cmake invocation
  set(ObtPipewire_DIR $ENV{ORKID_WORKSPACE_DIR}/cmake/configs)
  set(ObtPipewire_LIBRARIES ${ObtPipewireLibrary} )  
  set(ObtPipewire_INCLUDE_DIRS ${ObtPipewireIncludePath} ${ObtPipewireSpaIncludePath} )
  message( "ObtPipewire_DIR: " ${ObtPipewire_DIR} )
  message( "ObtPipewire_INCLUDE_DIRS: " ${ObtPipewire_INCLUDE_DIRS} )
  message( "ObtPipewire_LIBRARIES: " ${ObtPipewire_LIBRARIES} )
endif()

mark_as_advanced(ObtPipewire_DIR ObtPipewire_LIBRARIES ObtPipewire_INCLUDE_DIRS )


