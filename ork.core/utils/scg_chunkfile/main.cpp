#include <ork/file/scg_chunkfile.h>
#include <ork/file/path.h>
#include <boost/program_options.hpp>
#include <iostream>

namespace po = ::boost::program_options;

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto desc = std::make_shared<po::options_description>("SCG chunkfile tool");

  ////////////////////////////////////

  auto rval =                          //
      desc->add_options()              //
      ("help", "produce help message") //
      ("chunkfile", po::value<std::string>()->default_value(""), "chunkfile to process");

  ////////////////////////////////////

  auto vars = std::make_shared<po::variables_map>();
  if (desc) {
    auto cmdline = po::parse_command_line(argc, argv, *desc);
    po::store(cmdline, *vars);
    po::notify(*vars);
  }
  if (vars->count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }

  //////////////////////////////////////////////////////////////

  using namespace ork;

  auto chunkfile_path = file::Path((*vars)["chunkfile"].as<std::string>());

  bool does_exist = FileEnv::DoesFileExist(chunkfile_path);
  OrkAssert(does_exist);
  auto chfil = std::make_shared<scg::ChunkFile>(chunkfile_path.c_str());
  auto topgrp = chfil->_top_chunkgroup;
  topgrp->dump_headers("TOP");

  printf("Processing chunkfile<%s> header<%p>\n", chunkfile_path.c_str(), (void*) chfil.get() );

  auto map_chunk = topgrp->getChunkOfType("MAP_");
  auto cel_chunk = topgrp->getChunkOfType("CELS");
  auto map_data = topgrp->load(map_chunk);
  auto cel_data = topgrp->load(cel_chunk);
  auto map_group = chfil->_loadSubHeader(map_data);
  auto cel_group = chfil->_loadSubHeader(cel_data);
  map_group->dump_headers("MAP_");
  cel_group->dump_headers("CELS");
  auto map_hdr_chunk = map_group->getChunkOfType("HEAD");
  auto map_hdr_data = map_group->load(map_hdr_chunk);
  auto map_hdr_iter = map_hdr_data->iterator();

  uint32_t world_x_dim = map_hdr_iter->readItem<uint32_t>();
  uint32_t world_y_dim = map_hdr_iter->readItem<uint32_t>();
  uint32_t world_z_dim = map_hdr_iter->readItem<uint32_t>();
  uint32_t world_cel_count = world_x_dim*world_y_dim*world_z_dim;
  printf( "world_x_dim<%d>\n", world_x_dim );
  printf( "world_y_dim<%d>\n", world_y_dim );
  printf( "world_z_dim<%d>\n", world_z_dim );
  printf( "world_gridcell_count<%d>\n", world_cel_count );

  return 0;
}