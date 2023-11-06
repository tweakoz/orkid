cmake_minimum_required(VERSION 3.15)
project(MyHoudiniPlugin VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Typically, the Houdini environment variables would be:
# - $HFS: The Houdini install path
# - $HOUDINI_VERSION: The version of Houdini
# - $HOUDINI_BUILD_KERNEL: The build kernel of Houdini
# Assuming they're already set, we can use them directly.

add_definitions(-D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
# Set Houdini directories from the environment variables
set(HOUDINI_INCLUDE_DIR "$ENV{HFS}/toolkit/include")
set(HOUDINI_LIB_DIR "$ENV{HFS}/dsolib")

list( APPEND CMAKE_PREFIX_PATH "$ENV{HFS}/toolkit/cmake" )
find_package( Houdini REQUIRED )

set( library_name ork_rop )
add_library( ${library_name} SHARED
    rop_test.cpp
    rop_test.h
)
# Link against the Houdini libraries, and add required include directories and compile definitions.
target_link_directories( ${library_name} PUBLIC "$ENV{HFS}/../Libraries/" )
target_link_libraries( ${library_name} Houdini HoudiniPRM )
# Configure several common target properties, such as its output directory.
houdini_configure_target( ${library_name} )

# Set the installation prefix to Houdini's user preferences directory
if(UNIX AND NOT APPLE)
    set(HOUDINI_USER_PREF_DIR "$ENV{HOME}/houdini${HOUDINI_VERSION}")
elseif(APPLE)
    set(HOUDINI_USER_PREF_DIR "$ENV{HOME}/Library/Preferences/houdini/${HOUDINI_VERSION}")
endif()

set(CMAKE_INSTALL_PREFIX ${HOUDINI_USER_PREF_DIR})

# ... your existing CMake configuration ...

# Install the plugin to the dso subdirectory of the user preferences directory
install(TARGETS  ${library_name}
    LIBRARY DESTINATION dso
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
