#include <ork/asset/Asset.h>
#include <ork/asset/AssetLoader.h>
#include <ork/object/ObjectClass.h>
#include <ork/file/path.h>
#include <set>
namespace ork::asset {
////////////////////////////////////////////////////////////////////////////////
template <typename AssetType> //
inline void registerLoader(loader_ptr_t loader) {
  // todo support multiple loaders per asset type
  // todo support flyweighting
  auto assetclazz = dynamic_cast<object::ObjectClass*>(AssetType::GetClassStatic());
  object::ObjectClass::anno_t anno;
  anno.Set<asset::loader_ptr_t>(loader);
  assetclazz->annotate("ork.asset.loader", anno);
}
////////////////////////////////////////////////////////////////////////////////
template <typename AssetType> //
inline loader_ptr_t getLoader() {
  auto assetclazz = dynamic_cast<object::ObjectClass*>(AssetType::GetClassStatic());
  auto loaderanno = assetclazz->annotation("ork.asset.loader");
  auto loader     = loaderanno.Get<asset::loader_ptr_t>();
  return loader;
}
////////////////////////////////////////////////////////////////////////////////
inline loader_ptr_t getLoader(rtti::Class* clazz) {
  auto assetclazz = dynamic_cast<object::ObjectClass*>(clazz);
  auto loaderanno = assetclazz->annotation("ork.asset.loader");
  auto loader     = loaderanno.Get<asset::loader_ptr_t>();
  return loader;
}
} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
using namespace serdes;
template <> //
inline void ::ork::reflect::ITyped<asset::asset_ptr_t>::serialize(serdes::node_ptr_t sernode) const {
  auto serializer = sernode->_serializer;
  auto instance   = sernode->_ser_instance;
  asset::asset_ptr_t value;
  get(value, instance);
  auto as_asset = std::dynamic_pointer_cast<asset::Asset>(value);
  if (as_asset) {
    auto mapnode           = serializer->pushNode(_name, serdes::NodeType::MAP);
    mapnode->_parent       = sernode;
    mapnode->_ser_instance = instance;
    serializeMapSubLeaf<std::string>(mapnode, "class", as_asset->type());
    serializeMapSubLeaf<std::string>(mapnode, "path", as_asset->_name.toStdString());
    serializer->popNode(); // pop mapnode
  } else {
    sernode->_value.template Set<void*>(nullptr);
    serializer->serializeLeaf(sernode);
  }
}
template <> //
inline void ::ork::reflect::ITyped<asset::asset_ptr_t>::deserialize(serdes::node_ptr_t mapnode) const {
  using namespace serdes;
  auto deserializer = mapnode->_deserializer;
  auto instance     = mapnode->_deser_instance;

  std::string key1_out, key2_out;
  std::string val1 = deserializeMapSubLeaf<std::string>(mapnode, key1_out);
  std::string val2 = deserializeMapSubLeaf<std::string>(mapnode, key2_out);
  OrkAssert(key1_out == "class");
  OrkAssert(key2_out == "path");

  auto assetclazz = dynamic_cast<object::ObjectClass*>(Class::FindClass(val1.c_str()));
  auto loader     = asset::getLoader(assetclazz);
  if (loader->CheckAsset(val2)) {
    auto newobj   = assetclazz->createShared();
    auto newasset = std::dynamic_pointer_cast<asset::Asset>(newobj);
    OrkAssert(newasset->type() == val1);
    newasset->_name = val2;
    loader->LoadAsset(newasset);
    set(newasset, instance);
  } else {
    set(nullptr, instance);
  }
}
} // namespace ork::reflect
