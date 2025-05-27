cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.ecs.trace CXX)
#####################################################################################
setupEcsEXE (ork.example.ecs.tracewriter.exe main_writer.cpp )
setupEcsEXE (ork.example.ecs.tracereader.exe main_reader.cpp )
