project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
find_package (Python COMPONENTS Interpreter Development)
FIND_PACKAGE( pybind11 REQUIRED )

###################################

file(GLOB_RECURSE src_hdl ${SRCD}/hdl/*.cpp)

set( hdlsrcs ${src_hdl} )

add_compile_options(-mavx)

###################################

add_library (ork_eda SHARED ${hdlsrcs} )

set( libs ork_core )

###################################

target_link_libraries(ork_eda LINK_PRIVATE python3.8d ork_utpp )

target_include_directories (ork_eda PRIVATE ${INCD} ${COREINCD} )
target_include_directories( ork_eda PRIVATE ${PYTHON_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIRS} )
target_link_libraries(ork_eda PUBLIC ${libs}  tetgen )

install(TARGETS ork_eda LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )

###################################

#add_subdirectory (pyext)
add_subdirectory (tests)
add_subdirectory (utils/vcdviewer)
