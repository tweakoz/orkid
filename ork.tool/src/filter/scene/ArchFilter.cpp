////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/filter/filter.h>
#include <orktool/orktool_pch.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/asset/AssetManager.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////
bool ConvertArchetypeSbox2Arch(const tokenlist& toklist);
class ArchSandBoxExporter : public AssetFilterBase {
  RttiDeclareConcrete(ArchSandBoxExporter, AssetFilterBase);

public: //
  ArchSandBoxExporter() {}
  bool ConvertAsset(const tokenlist& toklist) final { return ConvertArchetypeSbox2Arch(toklist); }
};
///////////////////////////////////////////////////////////////////////////////
void ArchSandBoxExporter::Describe() {}
///////////////////////////////////////////////////////////////////////////////
void RegisterArchFilters() { AssetFilter::RegisterFilter("sbox2arch", ArchSandBoxExporter::DesignNameStatic().c_str()); }
///////////////////////////////////////////////////////////////////////////////
bool ConvertArchetypeSbox2Arch(const tokenlist& toklist) {
  ork::tool::FilterOptMap options;
  options.SetDefault("--in", "yo");
  options.SetDefault("--out", "yo");
  options.SetOptions(toklist);
  const std::string inf = options.GetOption("--in")->GetValue();
  const std::string outf = options.GetOption("--out")->GetValue();
  printf("ArchSandBoxExporter says yo in<%s> out<%s>\n", inf.c_str(), outf.c_str());

  /////////////////////////////////////////
  // disable autoload of some asset types
  /////////////////////////////////////////

  asset::AssetManager<lev2::TextureAsset>::DisableAutoLoad();
  // asset::AssetManager<lev2::XgmModelAsset>::DisableAutoLoad();
  // asset::AssetManager<lev2::FxShaderAsset>::DisableAutoLoad();

  ///////////////////

  stream::FileInputStream istream(inf.c_str());
  reflect::serialize::XMLDeserializer iser(istream);
  rtti::ICastable* pcastable = 0;
  bool bOK = iser.Deserialize(pcastable);

  if (bOK) {
    /////////////////////////////////////////
    ent::SceneData* pscene = rtti::autocast(pcastable);
    printf("SceneData<%p>\n", pscene);
    if (pscene) {
      orkmap<std::string, const ent::Archetype*> archs;
      const orkmap<PoolString, ent::SceneObject*>& sobjs = pscene->GetSceneObjects();
      printf("numofsobjs<%d>\n", int(sobjs.size()));
      for (orkmap<PoolString, ent::SceneObject*>::const_iterator item = sobjs.begin(); item != sobjs.end(); item++) {
        std::string name = item->first.c_str();
        const ent::SceneObject* pobj = item->second;

        const ent::Archetype* parch = ork::rtti::autocast(pobj);
        if (parch && name == "/arch/export") {
          archs[name] = parch;
        }
      }
      /////////////////////
      if (archs.size() != 1)
        return false;
      /////////////////////
      const std::string& name = archs.begin()->first;
      const ent::Archetype* parch = archs.begin()->second;
      printf("exporting archetype<%s:%p>\n", name.c_str(), parch);
      if (parch) {
        stream::FileOutputStream ostream(outf.c_str());
        reflect::serialize::XMLSerializer oser(ostream);
        oser.Serialize(parch);
      }
      /////////////////////
    }
  }
  ///////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::tool
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ArchSandBoxExporter, "ArchSandBoxExporter");
