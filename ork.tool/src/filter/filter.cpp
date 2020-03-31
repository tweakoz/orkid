////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <orktool/orktool_pch.h>

#include "aud/soundfont.h"
#include <ork/lev2/gfx/gfxenv.h>
#include <orktool/filter/filter.h>

#include <ork/file/filedev.h>
#include <ork/file/fileenv.h>

// Filters
#include <orktool/filter/gfx/meshutil/meshutil_tool.h>

#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/reflect/Functor.h>
#include <ork/rtti/downcast.h>
#include <orktool/toolcore/FunctionManager.h>
#include <ork/kernel/datacache.inl>

namespace ork::tool {
namespace meshutil {
void PartitionMesh_FixedGrid3d_Driver(const tokenlist& options);
void TerrainTest(const tokenlist& toklist);
void RegisterColladaFilters();
} // namespace meshutil

bool WavToMkr(const tokenlist& toklist);

void RegisterArchFilters();

void AssetFilterBase::Describe() {
}

#if defined(ORK_OSXX)
bool VolTexAssemble(const tokenlist& toklist);
bool QtzComposerToPng(const tokenlist& toklist);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class VolTexAssembleFilter : public AssetFilterBase {
  RttiDeclareConcrete(VolTexAssembleFilter, AssetFilterBase);

public: //
  VolTexAssembleFilter() {
  }
  bool ConvertAsset(const tokenlist& toklist) final {
    VolTexAssemble(toklist);
    return true;
  }
};
void VolTexAssembleFilter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class QtzComposerToPngFilter : public AssetFilterBase {
  RttiDeclareConcrete(QtzComposerToPngFilter, AssetFilterBase);

public: //
  QtzComposerToPngFilter() {
  }
  bool ConvertAsset(const tokenlist& toklist) final {
    QtzComposerToPng(toklist);
    return true;
  }
};
void QtzComposerToPngFilter::Describe() {
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool Tga2DdsFilterDriver(const tokenlist& toklist);

class TGADDSFilter : public AssetFilterBase {
  RttiDeclareConcrete(TGADDSFilter, AssetFilterBase);

public: //
  TGADDSFilter() {
  }
  bool ConvertAsset(const tokenlist& toklist) final {
    return Tga2DdsFilterDriver(toklist);
  }
};
void TGADDSFilter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class fg3dFilter : public AssetFilterBase {
  RttiDeclareConcrete(fg3dFilter, AssetFilterBase);

public: //
  fg3dFilter() {
  }
  bool ConvertAsset(const tokenlist& toklist) final {
    meshutil::PartitionMesh_FixedGrid3d_Driver(toklist);
    return true;
  }
};
void fg3dFilter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class WAVMKRFilter : public AssetFilterBase {
  RttiDeclareConcrete(WAVMKRFilter, AssetFilterBase);

public: //
  WAVMKRFilter() {
  }
  bool ConvertAsset(const tokenlist& toklist) final {
    return ork::tool::WavToMkr(toklist);
  }
};
void WAVMKRFilter::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static void RegisterFilters() {
  static bool binit = true;

  if (binit) {
    AssetFilter::RegisterFilter("wav:mkr", WAVMKRFilter::DesignNameStatic().c_str());
///////////////////////////////////////////////////
#if defined(_USE_SOUNDFONT)
    AssetFilter::RegisterFilter("sf2:xab", SF2XABFilter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("sf2:gab", SF2GABFilter::DesignNameStatic().c_str());
#endif
///////////////////////////////////////////////////
#if defined(USE_FCOLLADA)
    ork::tool::meshutil::RegisterColladaFilters();
#endif
    ///////////////////////////////////////////////////
    ork::tool::RegisterArchFilters();
    ///////////////////////////////////////////////////
    AssetFilter::RegisterFilter("xgm:obj", meshutil::XGM_OBJ_Filter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("obj:obj", meshutil::OBJ_OBJ_Filter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("obj:xgm", meshutil::OBJ_XGM_Filter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("ass:xgm", meshutil::ASS_XGM_Filter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("ass:xga", meshutil::ASS_XGA_Filter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("tga:dds", TGADDSFilter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("png:dds", TGADDSFilter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("fg3d", fg3dFilter::DesignNameStatic().c_str());
/////////////////////////
#if defined(ORK_OSXX_DISABLED)
    AssetFilter::RegisterFilter("qtz:png", QtzComposerToPngFilter::DesignNameStatic().c_str());
    AssetFilter::RegisterFilter("vtc:dds", VolTexAssembleFilter::DesignNameStatic().c_str());
#endif
    /////////////////////////
    binit = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

AssetFilterBase::AssetFilterBase() {
}

///////////////////////////////////////////////////////////////////////////////

orkmap<ork::PoolString, FilterInfo*> AssetFilter::smFilterMap;

void AssetFilter::RegisterFilter(const char* filtername, const char* classname, const char* pathmethod, const char* pathloc) {
  FilterInfo* pinfo = new FilterInfo;
  pinfo->filtername = AddPooledString(filtername);
  pinfo->classname  = AddPooledString(classname);
  pinfo->pathmethod = AddPooledString(pathmethod);
  pinfo->pathloc    = AddPooledString(pathloc);
  bool badded       = OldStlSchoolMapInsert(smFilterMap, AddPooledString(filtername), pinfo);
  OrkAssert(badded);
}

///////////////////////////////////////////////////////////////////////////////

bool AssetFilter::ConvertFile(const char* filter_name, const tokenlist& toklist) {
  bool rval = false;

  FilterInfo* info = OldStlSchoolFindValFromKey(smFilterMap, FindPooledString(filter_name), (FilterInfo*)nullptr);

  if (info) {
    PoolString classname = info->classname;

    rtti::Class* pclass = rtti::Class::FindClass(classname.c_str());

    OrkAssert(pclass != 0);

    AssetFilterBase* pfilter = rtti::safe_downcast<AssetFilterBase*>(pclass->CreateObject());

    OrkAssert(pfilter != 0);

    rval = pfilter->ConvertAsset(toklist);

    delete (pfilter);
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetFilter::ConvertTree(const char* Filter, const std::string& InTree, const std::string& OutDir) {
  OrkAssertNotImplI("AssetFilter::ConvertTree is not implemented!");
  return false;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetFilter::ListFilters() {
  int idx = 0;
  orkmessageh("///////////////////////////////////////\n");
  orkmessageh("// Orkid Filter List\n");
  orkmessageh("///////////////////////////////////////\n");
  for (orkmap<PoolString, FilterInfo*>::const_iterator it = smFilterMap.begin(); it != smFilterMap.end(); it++) {
    std::pair<PoolString, FilterInfo*> pr = *it;
    FilterInfo* pinfo                     = pr.second;
    orkmessageh("Filter %02d [%s]\n", idx, pinfo->filtername.c_str());
    idx++;
  }
  orkmessageh("///////////////////////////////////////\n");

  return true;
}

///////////////////////////////////////////////////////////////////////////////

class NullAppWindow : public ork::lev2::Window {
public: //
  NullAppWindow(int iX, int iY, int iW, int iH)
      : ork::lev2::Window(iX, iY, iW, iH) {
    initContext();
  }

  virtual void Draw(void) {
  }
  //	virtual void Show( void ) {};
  //	virtual void Hide( void ) {};
};

///////////////////////////////////////////////////////////////////////////////

int Main_Filter(tokenlist toklist) {
  RegisterFilters();

  FileEnv::SetFilesystemBase("./");

  //////////////////////////////////////////
  // Register fxshader:// data urlbase

  static auto FxShaderFileContext   = std::make_shared<FileDevContext>();
  file::Path::NameType fxshaderbase = ork::file::GetStartupDirectory() + "data/src/shaders/dummy";
  file::Path fxshaderpath(fxshaderbase.c_str());
  FxShaderFileContext->SetFilesystemBaseAbs(fxshaderpath.c_str());
  FxShaderFileContext->SetPrependFilesystemBase(true);

  FileEnv::registerUrlBase("fxshader://", FxShaderFileContext);

  //////////////////////////////
  // need a gfx context for some filters

  //	ork::lev2::GfxEnv::GetRef().SetCurrentRenderer( ork::lev2::EGFXENVTYPE_DUMMY );
  ork::lev2::GfxEnv::setContextClass(ork::lev2::ContextDummy::GetClassStatic());

  NullAppWindow* w = new NullAppWindow(0, 0, 640, 480);
  ork::lev2::GfxEnv::GetRef().RegisterWinContext(w);
  ork::lev2::GfxEnv::GetRef().SetLoaderTarget(w->context());

  //////////////////////////////

  bool blist = toklist.empty();

  tokenlist::iterator it = toklist.begin();
  it++;
  const std::string& ftype = *it++;

  if (blist || (ftype == (std::string) "list")) {
    AssetFilter::ListFilters();
  } else {
    FilterInfo* info = OldStlSchoolFindValFromKey(AssetFilter::smFilterMap, FindPooledString(ftype.c_str()), (FilterInfo*)0);
    printf("Main_Filter find<%s> finfo<%p>\n", ftype.c_str(), info);
    if (info) {
      tokenlist newtoklist;

      newtoklist.insert(newtoklist.begin(), it, toklist.end());
      for (auto tok : newtoklist) {
        if (tok == "--nocache") {
          DataBlockCache::_enabled = false;
        }
      }

      bool bret = AssetFilter::ConvertFile(ftype.c_str(), newtoklist);
      return bret ? 0 : -1;
    }
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////

int Main_FilterTree(tokenlist toklist) {
  RegisterFilters();

  FileEnv::SetFilesystemBase("./");

  //////////////////////////////
  // need a gfx context for some filters

  // ork::lev2::GfxEnv::GetRef().SetCurrentRenderer( ork::lev2::EGFXENVTYPE_DUMMY );
  ork::lev2::GfxEnv::setContextClass(ork::lev2::ContextDummy::GetClassStatic());
  NullAppWindow* w = new NullAppWindow(0, 0, 640, 480);

  //////////////////////////////

  std::string ftype, treename, outdest;

  toklist.erase(toklist.begin()); // absorb -filtertree

  if (false == toklist.empty()) {
    ftype = *toklist.begin();
    toklist.erase(toklist.begin());

    if (false == toklist.empty()) {
      treename = *toklist.begin();
      toklist.erase(toklist.begin());

      if (false == toklist.empty()) {
        outdest = *toklist.begin();
        toklist.erase(toklist.begin());
      }
    }
  }

  orkmessageh("Converting Directory Tree [%s]\n", treename.c_str());
  AssetFilter::ConvertTree(ftype.c_str(), treename, outdest);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////

void FilterOption::SetValue(const char* defval) {
  mValue = std::string(defval);
}

///////////////////////////////////////////////////////////////////////////////

void FilterOption::SetDefault(const char* defval) {
  mDefaultValue = std::string(defval);
}

const std::string& FilterOption::GetValue() const {
  if (0 == mValue.length()) {
    return mDefaultValue;
  } else
    return mValue;
}

///////////////////////////////////////////////////////////////////////////////

FilterOption* FilterOptMap::GetOption(const std::string& key) {
  orkmap<std::string, FilterOption>::iterator it = moptions_map.find(key);
  if (it != moptions_map.end()) {
    return &it->second;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

const FilterOption* FilterOptMap::GetOption(const std::string& key) const {
  orkmap<std::string, FilterOption>::const_iterator it = moptions_map.find(key);
  if (it != moptions_map.end()) {
    return &it->second;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

bool FilterOptMap::HasOption(const std::string& key) const {
  orkmap<std::string, FilterOption>::const_iterator it = moptions_map.find(key);
  return (it != moptions_map.end());
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::SetOptions(const tokenlist& options) {
  for (tokenlist::const_iterator it = options.begin(); it != options.end(); it++) {
    const std::string& key = (*it);

    if (key[0] == '-') {
      if (moptions_map.find(key) == moptions_map.end()) {
        moptions_map[key] = FilterOption(key.c_str(), "");
      }

      FilterOption* popt = GetOption(key);

      if (it != options.end()) {
        tokenlist::const_iterator itn = it;
        itn++;

        if (itn != options.end()) {
          const std::string& val = (*itn);

          if (val[0] != '-') {
            popt->SetValue(val.c_str());
            it++;
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::SetDefault(const char* name, const char* defval) {
  orkmap<std::string, FilterOption>::iterator it = moptions_map.find(std::string(name));

  if (it == moptions_map.end()) {
    moptions_map[std::string(name)] = FilterOption(name, defval);
  } else {
    it->second.SetDefault(defval);
  }
}

///////////////////////////////////////////////////////////////////////////////

FilterOptMap::FilterOptMap() {
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::DumpDefaults() const {
  orkprintf("/////////////////////////////\n");
  orkprintf("// Options (defaults)\n");

  for (orkmap<std::string, FilterOption>::const_iterator it = moptions_map.begin(); it != moptions_map.end(); it++) {
    const std::string& key  = it->first;
    const FilterOption& val = it->second;

    orkprintf("// Option key <%s> default<%s>\n", key.c_str(), val.GetDefault().c_str());
  }
  orkprintf("/////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::DumpOptions() const {
  orkprintf("/////////////////////////////\n");
  orkprintf("// Options (set values)\n");

  for (orkmap<std::string, FilterOption>::const_iterator it = moptions_map.begin(); it != moptions_map.end(); it++) {
    const std::string& key  = it->first;
    const FilterOption& val = it->second;

    orkprintf("// Option key <%s> value<%s>\n", key.c_str(), val.GetValue().c_str());
  }
  orkprintf("/////////////////////////////\n");
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::tool
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::AssetFilterBase, "AssetFilterBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::fg3dFilter, "fg3dFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::WAVMKRFilter, "WAVMKRFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::TGADDSFilter, "TGADDSFilter");

#if defined(ORK_OSXX_DISABLED)
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::VolTexAssembleFilter, "VolTexAssembleFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::QtzComposerToPngFilter, "QtzComposerToPngFilter");
#endif
