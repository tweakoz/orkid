////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil_tool.h>
#include <orktool/filter/gfx/collada/collada.h>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::meshutil::XGM_OBJ_Filter, "XGM_OBJ_Filter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::meshutil::OBJ_OBJ_Filter, "OBJ_OBJ_Filter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::meshutil::OBJ_XGM_Filter, "OBJ_XGM_Filter");

///////////////////////////////////////////////////////////////////////////////
namespace ork::tool::meshutil {
///////////////////////////////////////////////////////////////////////////////

void OBJ_XGM_Filter::Describe() {
}
void XGM_OBJ_Filter::Describe() {
}
void OBJ_OBJ_Filter::Describe() {
}

OBJ_XGM_Filter::OBJ_XGM_Filter() {
}
XGM_OBJ_Filter::XGM_OBJ_Filter() {
}
OBJ_OBJ_Filter::OBJ_OBJ_Filter() {
}

bool OBJ_XGM_Filter::ConvertAsset(const tokenlist& toklist) {
  tokenlist::const_iterator it = toklist.begin();
  const std::string& inf       = *it++;
  const std::string& outf      = *it++;
  ToolMesh tmesh;
  tmesh.ReadFromWavefrontObj(inf.c_str());
  tmesh.WriteToXgmFile(outf.c_str());
  return true;
}
bool XGM_OBJ_Filter::ConvertAsset(const tokenlist& toklist) {
  tokenlist::const_iterator it = toklist.begin();
  const std::string& inf       = *it++;
  const std::string& outf      = *it++;
  ToolMesh tmesh;
  tmesh.ReadFromXGM(inf.c_str());
  tmesh.WriteToWavefrontObj(outf.c_str());
  return true;
}
bool OBJ_OBJ_Filter::ConvertAsset(const tokenlist& toklist) {
  tokenlist::const_iterator it = toklist.begin();
  const std::string& inf       = *it++;
  const std::string& outf      = *it++;
  ToolMesh tmesh;
  tmesh.ReadFromWavefrontObj(inf.c_str());
  tmesh.WriteToWavefrontObj(outf.c_str());
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void ToolMesh::ReadAuto(const file::Path& BasePath) {
  ork::file::Path::SmallNameType ext = BasePath.GetExtension();

  if (ext == "dae") {
    DaeReadOpts opts;
    ReadFromDaeFile(BasePath, opts);
  } else if (ext == "obj") {
    ReadFromWavefrontObj(BasePath);
  } else if (ext == "xgm") {
    ReadFromXGM(BasePath);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ToolMesh::WriteAuto(const file::Path& BasePath) const {
  ork::file::Path::SmallNameType ext = BasePath.GetExtension();

  if (ext == "dae") {
    DaeWriteOpts out_opts;
    WriteToDaeFile(BasePath, out_opts);
  } else if (ext == "obj") {
    WriteToWavefrontObj(BasePath);
  }
}

void PartitionMesh_FixedGrid3d(const ToolMesh& MeshIn, orkvector<ToolMesh*>& OutMeshes) {
  /*GridGraph thegraph;

  thegraph.BeginPreMerge();
  {
      thegraph.PreMergeMesh( MeshIn );
  }
  thegraph.EndPreMerge();

  Mesh OutMesh;

  thegraph.MergeMesh( MeshIn,OutMesh );

  //const Mesh& outmesh = thegraph.RefOutMesh();

  if( OutMesh.GetNumPolys() > 0 )
  {
      ork::file::Path outpath( "tmp/yo.obj" );

      OutMesh.WriteToWavefrontObj( outpath );
  }*/
}

void PartitionMesh_FixedGrid3d_Driver(const tokenlist& options) {
  ////////////////////////////////////////

  ork::tool::FilterOptMap OptionsMap;
  OptionsMap.SetDefault("-inobj", "partition_in.obj");
  OptionsMap.SetDefault("-outobj", "partition_out.obj");
  OptionsMap.SetOptions(options);

  OptionsMap.DumpOptions();

  ToolMesh inmesh;

  std::string uva_in  = OptionsMap.GetOption("-inobj")->GetValue();
  std::string uva_out = OptionsMap.GetOption("-outobj")->GetValue();

  inmesh.ReadAuto(uva_in.c_str());

  orkprintf("uvain<%s> uvaout<%s>\n", uva_in.c_str(), uva_out.c_str());

  orkvector<ToolMesh*> OutMeshes;

  PartitionMesh_FixedGrid3d(inmesh, OutMeshes);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::tool::meshutil
///////////////////////////////////////////////////////////////////////////////
