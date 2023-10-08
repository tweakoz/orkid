#include <ork/file/scg_chunkfile.h>
#include <ork/file/path.h>
#include <boost/program_options.hpp>
#include <ork/util/hexdump.inl>
#include <ork/util/endian.h>
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


//////////////////////////////////////////////////////////////

  using namespace ork;

  auto chunkfile_path = file::Path((*vars)["chunkfile"].as<std::string>());

  bool does_exist = FileEnv::DoesFileExist(chunkfile_path);
  OrkAssert(does_exist);

  auto chfil  = std::make_shared<scg::ChunkFile>(chunkfile_path.c_str());
  auto topgrp = chfil->_top_chunkgroup;
  topgrp->dump_headers("TOP");

  printf("Processing chunkfile<%s> header<%p>\n", chunkfile_path.c_str(), (void*)chfil.get());

  auto map_chunk = topgrp->getChunkOfType("MAP_");
  if (map_chunk) {
    auto map_data  = topgrp->load(map_chunk);
    auto map_group = chfil->_loadSubHeader(map_data);
    map_group->dump_headers("MAP_");

    printf( "MAP_ version code<%s>\n", map_data->_versionCode.c_str() );

    /////////////////////////////////////////////////////////////////
    // heuristics for SCG worldbuilder map file version detection
    //  apparently we were not setting version codes in chunks
    //  despite having the field available.
    //  so => we need some heuristics
    /////////////////////////////////////////////////////////////////

    auto head = map_group->getChunkOfType("HEAD"); // HEAD (world dimensions)
    auto bspv = map_group->getChunkOfType("BSPV"); // BSP PVS data array ?
    auto vars = map_group->getChunkOfType("VARS"); // level vars ?

    bool probably_SUPES = (bspv!=nullptr);
    bool probably_ET = (not probably_SUPES) and (vars==nullptr);
    bool probably_PICKLES = (not probably_SUPES) and (vars!=nullptr);

    printf( "MAP_ probably_ET<%d>\n", int(probably_ET) );
    printf( "MAP_ probably_PICKLES<%d>\n", int(probably_PICKLES) );
    printf( "MAP_ probably_SUPES<%d>\n", int(probably_SUPES) );

    /////////////////////////////////////////////////////////////////

    auto map_hdr_data  = map_group->load(head);
    auto map_hdr_iter  = map_hdr_data->iterator();

    uint32_t world_x_dim     = map_hdr_iter->readItem<uint32_t>();
    uint32_t world_y_dim     = map_hdr_iter->readItem<uint32_t>();
    uint32_t world_z_dim     = map_hdr_iter->readItem<uint32_t>();
    uint32_t world_cel_count = world_x_dim * world_y_dim * world_z_dim;

    printf("world_x_dim<%d>\n", world_x_dim);
    printf("world_y_dim<%d>\n", world_y_dim);
    printf("world_z_dim<%d>\n", world_z_dim);
    printf("world_gridcell_count<%d>\n", world_cel_count);

    /////////////////////////////////////////////////////////////////

    if(vars){

        auto vars_data  = map_group->load(vars);

        if(probably_SUPES){
            OrkAssert(vars_data->_versionCode == "0001");
        }
        else if( probably_PICKLES ){
            OrkAssert(vars_data->_versionCode == "0000");
        }
        else {
            OrkAssert(false);
        }

        hexdumpbytes(vars_data->_data);
        auto vars_iter = vars_data->iterator();
        bool done = false;


        while(not done){
            uint32_t tag = vars_iter->readItem<uint32_t>();
            //swapbytes(tag);
            done = (vars_iter->_offset >= vars_data->_data.size()) or (tag == 0xffffffff);
            if(not done){
                uint32_t len = (tag>>24)&0xff;
                if(len==0){
                    len = 3;
                }
                //len += 1;

                uint32_t type = (tag>>16)&0xff;
                uint32_t id   = (tag&0xffff);

                printf( "tag<%08x> len<%d> id<%d>: ", tag, len, id );
                switch(type){
                    case 0:{
                        auto bool_val = vars_iter->readItem<uint32_t>();
                        printf( "BOOL<%d>", bool_val);
                        break;
                    }
                    case 1:{
                        auto s32_val = vars_iter->readItem<int32_t>();
                        printf( "S32<%d>", s32_val);
                        break;
                    }
                    case 2:{
                        auto x = vars_iter->readItem<int32_t>();
                        auto y = vars_iter->readItem<int32_t>();
                        auto z = vars_iter->readItem<int32_t>();
                        float fx = float(x)/float(1<<15);
                        float fy = float(y)/float(1<<15);
                        float fz = float(z)/float(1<<15);
                        printf( "VECTOR<%g %g %g>", fx,fy,fz);
                        break;
                    }
                    case 3:{
                        auto s32_val = vars_iter->readItem<int32_t>();
                        printf( "CHOICE<%d>", s32_val);
                        break;
                    }
                    case 4:{
                        // string
                        OrkAssert(false);
                        break;
                    }
                    case 5:{
                        // u32
                        OrkAssert(false);
                        break;
                    }
                    case 6:{
                        // fixed32
                        OrkAssert(false);
                        break;
                    }
                    default:
                        OrkAssert(false);
                        break;
                }
                printf( "\n");
                //vars_iter->_offset += len*sizeof(uint32_t);
            }
        }
    }
  } // if (map_chunk) {

  auto cel_chunk = topgrp->getChunkOfType("CELS");
  if(cel_chunk){
      auto cel_data  = topgrp->load(cel_chunk);
    auto cel_group = chfil->_loadSubHeader(cel_data);
    cel_group->dump_headers("CELS");
  }


  return 0;
}