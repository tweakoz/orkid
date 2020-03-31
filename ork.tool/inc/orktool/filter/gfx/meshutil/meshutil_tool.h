////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <orktool/filter/filter.h>

namespace ork::tool::meshutil {
struct ColladaMaterial;
struct DaeReadOpts;
struct DaeWriteOpts;

// TODO use loose functions instead of subclass

class ToolMesh : public ork::meshutil::Mesh {
public:
  void WriteToDaeFile(const file::Path& outpath, const DaeWriteOpts& writeopts) const;
  void ReadFromDaeFile(const file::Path& inpath, DaeReadOpts& readopts);
  void readFromAssimp(const file::Path& inpath, DaeReadOpts& readopts);
  void ReadAuto(const file::Path& outpath);
  void WriteAuto(const file::Path& outpath) const;
};

///////////////////////////////////////////////////////////////////////////////

class OBJ_OBJ_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(OBJ_OBJ_Filter, ork::tool::AssetFilterBase);

public: //
  OBJ_OBJ_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};
class XGM_OBJ_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(XGM_OBJ_Filter, ork::tool::AssetFilterBase);

public: //
  XGM_OBJ_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};
class OBJ_XGM_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(OBJ_XGM_Filter, ork::tool::AssetFilterBase);

public: //
  OBJ_XGM_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};

class ASS_XGM_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(ASS_XGM_Filter, ork::tool::AssetFilterBase);

public: //
  ASS_XGM_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};
class ASS_XGA_Filter : public ork::tool::AssetFilterBase {
  RttiDeclareConcrete(ASS_XGA_Filter, ork::tool::AssetFilterBase);

public: //
  ASS_XGA_Filter();
  bool ConvertAsset(const tokenlist& toklist) final;
};

} // namespace ork::tool::meshutil
