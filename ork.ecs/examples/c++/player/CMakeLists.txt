cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.ecs.player CXX)
#####################################################################################
# ork.ecs.player.exe — the pure-C++ HYPERECS scene player (D.5 / Goal-C acceptance).
#####################################################################################
setupEcsEXE (ork.ecs.player.exe main.cpp )
