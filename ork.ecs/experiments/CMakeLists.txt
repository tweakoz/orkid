cmake_minimum_required (VERSION 3.21.0)
include(orkid)
project (ork.ecs.experiment CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )

set(PREBUILT_MODULE_PATH ${CMAKE_BINARY_DIR}/modules)

#####################################################3

set( MODULE_FLAG_LIST "" )

list(APPEND MODULE_FLAG_LIST -fmodules)
list(APPEND MODULE_FLAG_LIST -fimplicit-modules)
list(APPEND MODULE_FLAG_LIST -fimplicit-module-maps)
list(APPEND MODULE_FLAG_LIST -fbuiltin-module-map)
list(APPEND MODULE_FLAG_LIST -fprebuilt-module-path=${PREBUILT_MODULE_PATH})
list(APPEND MODULE_FLAG_LIST -v)

#####################################################3

set( MODULE_STD -std=gnu++2a )

set( MODULE_INCDIR_LIST "" )
list(APPEND MODULE_INCDIR_LIST -I${SRCD} )
list(APPEND MODULE_INCDIR_LIST -I$ENV{OBT_STAGE}/include/luajit-2.1 )
list(APPEND MODULE_INCDIR_LIST -I${ORK_CORE_INCD} )
list(APPEND MODULE_INCDIR_LIST -I${ORK_LEV2_INCD} )
list(APPEND MODULE_INCDIR_LIST -I${ORK_ECS_INCD} )
list(APPEND MODULE_INCDIR_LIST -I$ENV{OBT_STAGE}/orkid/modules )

#####################################################3

function(add_module module_name ixx_file )
    file(MAKE_DIRECTORY ${PREBUILT_MODULE_PATH})
    set( PCM_OUTPUT_NAME ${module_name}.pcm )
    add_custom_target( ${PCM_OUTPUT_NAME} ALL
      DEPENDS ${ixx_file}
    	COMMAND ${CMAKE_CXX_COMPILER}
      $<JOIN:$<TARGET_PROPERTY:${PCM_OUTPUT_NAME},TGT_DEFINITIIONS>,\t>
      -I$<JOIN:$<TARGET_PROPERTY:${PCM_OUTPUT_NAME},TGT_INCLUDE_PATHS>,\t-I>
			${MODULE_INCDIR_LIST}
      $<JOIN:$<TARGET_PROPERTY:${PCM_OUTPUT_NAME},TGT_OPTIONS>,\t>
      -O3 -DNDEBUG -mavx
      ${MODULE_FLAG_LIST}
      ${MODULE_STD}
      -fmodule-map-file=${TESTSRCD}/ork_corex.modulemap
      -fmodule-name=${module_name}
			-x c++
      -Xclang -emit-module-interface
      -c ${ixx_file}
      -o ${PREBUILT_MODULE_PATH}/${PCM_OUTPUT_NAME}
    )
    ork_std_target_opts_compiler_module(${PCM_OUTPUT_NAME})
		add_dependencies(${PCM_OUTPUT_NAME} ork_core ork_lev2 ork_ecs )
endfunction()

#####################################################3

function(add_headerunit_module module_name ihh_file module_map )
    file(MAKE_DIRECTORY ${PREBUILT_MODULE_PATH})
    set( PCM_OUTPUT_NAME ${module_name}.pcm )
    add_custom_target( ${PCM_OUTPUT_NAME} ALL
      DEPENDS ${ihh_file}
      COMMAND ${CMAKE_CXX_COMPILER}
      $<JOIN:$<TARGET_PROPERTY:${PCM_OUTPUT_NAME},TGT_DEFINITIIONS>,\t>
      -I$<JOIN:$<TARGET_PROPERTY:${PCM_OUTPUT_NAME},TGT_INCLUDE_PATHS>,\t-I>
      ${MODULE_INCDIR_LIST}
      $<JOIN:$<TARGET_PROPERTY:${PCM_OUTPUT_NAME},TGT_OPTIONS>,\t>
      -O3 -DNDEBUG -mavx
      ${MODULE_FLAG_LIST}
      ${MODULE_STD}
      -x c++
      -Xclang -emit-header-module 
      -fmodule-name=${module_name}
      -c ${ihh_file}
      -o ${PREBUILT_MODULE_PATH}/${PCM_OUTPUT_NAME}
    )
    ork_std_target_opts_compiler_module(${PCM_OUTPUT_NAME})
    add_dependencies(${PCM_OUTPUT_NAME} ork_core ork_lev2 ork_ecs )
endfunction()

#####################################################3

#clang++.exe -cc1 module.modulemap -o prebuilt/mod.pcm -emit-module -fmodules -fmodule-name=mod -std=c++20 ^
#    -internal-isystem "d:\\bin2\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.28.29910\\include" ^
#    -internal-isystem "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.19041.0\\ucrt" ^
#    -debug-info-kind=limited -fms-extensions -fms-compatibility -fms-compatibility-version=19.28 -xc++ ^
#    -fmath-errno -fexceptions -fcxx-exceptions -triple x86_64-pc-windows-msvc || exit /b
    
#####################################################3

function(uses_modules the_target module_list )
	ork_std_target_opts(${the_target})
	target_compile_options( ${the_target} PRIVATE ${MODULE_FLAG_LIST})
	target_compile_options( ${the_target} PRIVATE ${MODULE_STD})
  target_compile_options( ${the_target} PRIVATE -fmodule-map-file=${TESTSRCD}/ork_corex.modulemap )
  #target_compile_options( ${the_target} PRIVATE -fmodule-file=ork_corex=${PREBUILT_MODULE_PATH}/ork_corex.pcm )
	target_compile_options (${the_target} PRIVATE ${MODULE_INCDIR_LIST} )
  list(TRANSFORM module_list APPEND .pcm )
  add_dependencies(${the_target} ${module_list} )

endfunction()

#####################################################3
# these need to be global for now until we can figure
#   out how to make the target scoped settings
#   work with our custom target.command
#####################################################3

file(GLOB testsrcs ${TESTSRCD}/*.cpp)

#####################################################3

add_headerunit_module(ork_corex ${TESTSRCD}/ork_corex.ihh ork_corex.modulemap )
add_module(stl ${TESTSRCD}/stl.ixx)
add_module(ork ${TESTSRCD}/ork.ixx)
add_module(mtest ${TESTSRCD}/mtest.ixx)

set( module_list stl )
list(APPEND module_list ork_corex )
list(APPEND module_list ork )
list(APPEND module_list mtest )

#####################################################3

add_executable (ork.experiment.ecs.exe ${testsrcs} )
uses_modules(ork.experiment.ecs.exe ${module_list} )

#####################################################3

target_link_libraries(ork.experiment.ecs.exe LINK_PRIVATE ork_utpp ork_core ork_lev2 ork_ecs )
set_target_properties(ork.experiment.ecs.exe PROPERTIES LINKER_LANGUAGE CXX)
install(TARGETS ork.experiment.ecs.exe DESTINATION $ENV{OBT_STAGE}/bin )

#####################################################3
