////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/filter/filter.h>
#include <ork/kernel/spawner.h>
#include <ork/lev2/gfx/texman.h>

namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

bool NvttCompress( const ork::tool::FilterOptMap& options ) ;

bool Tga2DdsFilterDriver( const tokenlist& toklist )
{
	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault( "--in", "yo.tga" );
	OptionsMap.SetDefault( "--out", "" );
	OptionsMap.SetDefault( "-mipfilter", "kaiser" );
	OptionsMap.SetDefault( "-platform", "pc" );
	OptionsMap.SetOptions( toklist );

	return NvttCompress( OptionsMap );

}

bool NvttCompress( const ork::tool::FilterOptMap& OptionsMap )
{
  std::string tex_in = OptionsMap.GetOption( "--in" )->GetValue();
  std::string tex_out = OptionsMap.GetOption( "--out" )->GetValue();
  file::Path inpath( tex_in.c_str() );
  file::Path outpath( tex_out.c_str() );

  /////////////////////////////////////////////////////
  // implicit output path
  /////////////////////////////////////////////////////

  if( tex_out == "" ){

    printf( "inpath<%s>\n", inpath.c_str() );

    std::vector<std::string> out_nodes;
    SplitString(tex_in, '/',out_nodes);

    auto it = std::find( out_nodes.begin(),
                   out_nodes.end(),
                   "textures");

    if( it != out_nodes.end() ){
      out_nodes.erase(it);
      outpath = file::Path(out_nodes);
    }
    it = std::find( out_nodes.begin(),
                    out_nodes.end(),
                    "src");
    if( it != out_nodes.end() ){
      (*it) = "pc";
      outpath = file::Path(out_nodes);
    }

    outpath.SetExtension("dds");

    printf( "outpath<%s>\n", outpath.c_str() );

  }

  /////////////////////////////////////////////////////

  ork::lev2::invoke_nvcompress( inpath.ToAbsolute().c_str(),
                                outpath.ToAbsolute().c_str(),
                                "bc3" );

	return true;
}


///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
