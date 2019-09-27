project (ork CXX)
include (GenerateExportHeader)

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( LEV2INCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.lev2/inc )
###################################

file(GLOB_RECURSE src_event ${SRCD}/event/*.cpp)
file(GLOB_RECURSE src_luaintf ${SRCD}/LuaIntf/*.cpp)
file(GLOB_RECURSE src_ent ${SRCD}/*.cpp)

set( orksrcs ${src_event} ${src_luaintf} ${src_ent} )

###################################

add_library (ork_ecs SHARED ${orksrcs})

###################################

set( libs ork_core ork_lev2 ork_tuio )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio luajit-5.1 )
list(APPEND libs BulletCollision BulletDynamics LinearMath )
list(APPEND libs boost_filesystem )
IF(${APPLE})
    target_include_directories (ork_ecs PRIVATE /usr/local/include)
    list(APPEND libs m pthread )
ELSEIF(${UNIX})
    list(APPEND libs rt dl pthread GL )
ENDIF()

target_link_libraries(ork_ecs ${libs})

target_include_directories (ork_ecs PRIVATE ${INCD} )
target_include_directories (ork_ecs PRIVATE ${SRCD} )
target_include_directories (ork_ecs PRIVATE ${SRCD}/LuaIntf )
target_include_directories (ork_ecs PRIVATE ${COREINCD} )
target_include_directories (ork_ecs PRIVATE ${LEV2INCD} )
target_include_directories (ork_ecs PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_ecs PRIVATE $ENV{OBT_STAGE}/include/luajit-2.0 )
target_include_directories (ork_ecs PRIVATE $ENV{OBT_STAGE}/include/bullet )

###################################


install(TARGETS ork_ecs LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )
