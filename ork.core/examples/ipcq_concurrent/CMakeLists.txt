cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.ipcq.concurrent CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

add_executable (_ork.ipcq.simple.exe test_simple.cpp )
add_executable (_ork.ipcq.debug.exe debug_test.cpp )
add_executable (_ork.ipcq.sequence_validation.exe test_sequence_validation.cpp )
add_executable (_ork.shmobject.stress.exe test_shmobject_stress.cpp )

ork_std_target_opts(_ork.ipcq.simple.exe)
ork_std_target_opts(_ork.ipcq.debug.exe)
ork_std_target_opts(_ork.ipcq.sequence_validation.exe)
ork_std_target_opts(_ork.shmobject.stress.exe)

target_link_libraries(_ork.ipcq.simple.exe LINK_PRIVATE ork_core)
target_link_libraries(_ork.ipcq.debug.exe LINK_PRIVATE ork_core)
target_link_libraries(_ork.ipcq.sequence_validation.exe LINK_PRIVATE ork_core)
target_link_libraries(_ork.shmobject.stress.exe LINK_PRIVATE ork_core)

set_target_properties(_ork.ipcq.simple.exe PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(_ork.ipcq.debug.exe PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(_ork.ipcq.sequence_validation.exe PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(_ork.shmobject.stress.exe PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories (_ork.ipcq.simple.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (_ork.ipcq.simple.exe PRIVATE ${SRCD} )
target_include_directories (_ork.ipcq.debug.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (_ork.ipcq.debug.exe PRIVATE ${SRCD} )
target_include_directories (_ork.ipcq.sequence_validation.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (_ork.ipcq.sequence_validation.exe PRIVATE ${SRCD} )
target_include_directories (_ork.shmobject.stress.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (_ork.shmobject.stress.exe PRIVATE ${SRCD} )

install(TARGETS _ork.ipcq.simple.exe DESTINATION $ENV{OBT_STAGE}/bin )
install(TARGETS _ork.ipcq.debug.exe DESTINATION $ENV{OBT_STAGE}/bin )
install(TARGETS _ork.ipcq.sequence_validation.exe DESTINATION $ENV{OBT_STAGE}/bin )
install(TARGETS _ork.shmobject.stress.exe DESTINATION $ENV{OBT_STAGE}/bin )