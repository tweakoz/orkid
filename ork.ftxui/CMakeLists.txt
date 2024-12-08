project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src/ftxui )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/include )

#message( ${SRCD} )
#message( ${INCD} )

###################################

file(GLOB_RECURSE src_component ${SRCD}/component/*.cpp)
file(GLOB_RECURSE src_dom ${SRCD}/dom/*.cpp)
file(GLOB_RECURSE src_screen ${SRCD}/screen/*.cpp)

#message( ${src_component})
#message( ${src_dom})
#message( ${src_screen})

set( ftxuisrc ${src_component} ${src_dom} ${src_screen} )

#message( ${ftxuisrc})
###################################

add_library (ork_ftxui SHARED ${ftxuisrc} )

ork_std_target_opts(ork_ftxui)
target_include_directories (ork_ftxui PRIVATE ${INCD} )

#enable_python_on_target(ork_ftxui)
#target_link_libraries(ork_ftxui LINK_PRIVATE  )

#target_link_libraries(ork_ftxui LINK_PRIVATE Boost::filesystem xxhash )
#target_link_libraries(ork_ftxui LINK_PUBLIC ${ObtOpenBlas_LIBRARIES} )

#find_package(Python COMPONENTS Interpreter Development REQUIRED)
#target_include_directories (ork_ftxui PRIVATE ${INCD} )

install(TARGETS ork_ftxui LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )

###################################

#add_subdirectory (pyext)
#add_subdirectory (examples)
#add_subdirectory (tests)
#add_subdirectory (utils)
