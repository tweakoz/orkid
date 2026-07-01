cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.ecs.player CXX)
#####################################################################################
# ork.ecs.player.exe — the pure-C++ HYPERECS scene player (D.5 / Goal-C acceptance).
#####################################################################################
setupEcsEXE (ork.ecs.player.exe main.cpp )
# the player parses argv with boost::program_options (command_line_parser/store/notify
# are in the compiled lib; ork_std_target_opts_* only link Boost::system PRIVATE).
target_link_libraries (ork.ecs.player.exe LINK_PRIVATE Boost::program_options)
