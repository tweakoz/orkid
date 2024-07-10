project (orkecs CXX)
include (GenerateExportHeader)

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( LEV2INCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.lev2/inc )
###################################

#file(GLOB_RECURSE src_event ${SRCD}/event/*.cpp)
#file(GLOB_RECURSE src_luaintf ${SRCD}/LuaIntf/*.cpp)
file(GLOB_RECURSE src_ecs ${SRCD}/*.cpp)

#set( orksrcs ${src_event} ${src_luaintf} ${src_ecs} )
set( orksrcs ${src_ecs} )

#find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

###################################

add_library (ork_ecs SHARED ${orksrcs})
ork_std_target_opts(ork_ecs)

###################################

set( libs ork_core ork_lev2 )
#list(APPEND libs OpenImageIO OpenImageIO_Util )
list(APPEND libs BulletCollision BulletDynamics LinearMath )
list(APPEND libs luajit-5.1 )
IF(${APPLE})
    target_include_directories (ork_ecs PRIVATE /usr/local/include)
    list(APPEND libs m pthread )
ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread GL )
ENDIF()
list(APPEND libs luajit-5.1 )

message( WTF )
message( ${Python3_LIBRARIES} )

enable_python_on_target(ork_ecs)

target_link_libraries(ork_ecs PRIVATE ${libs} )

target_include_directories (ork_ecs PRIVATE ${INCD} )
target_include_directories (ork_ecs PRIVATE ${SRCD} )
target_include_directories (ork_ecs PRIVATE ${SRCD}/LuaIntf )
target_include_directories (ork_ecs PRIVATE ${COREINCD} )
target_include_directories (ork_ecs PRIVATE ${LEV2INCD} )
target_include_directories (ork_ecs PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_ecs PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
target_include_directories (ork_ecs PRIVATE $ENV{OBT_STAGE}/include/bullet )
###################################


install(TARGETS ork_ecs LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )

add_subdirectory (tests)
add_subdirectory (examples)
#add_subdirectory (experiments)
add_subdirectory (pyext)
add_subdirectory (pyext_sim)
