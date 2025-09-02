
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/asset/Asset.h>
#include <ork/asset/AssetSetEntry.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/request.h>
#include <ork/reflect/properties/registerX.inl>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::asset::Asset, "Asset");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

void LoadSynchronizer::increment(){
  int count = _load_counter.fetch_add(1);
  printf("LoadSynchronizer::increment count<%d>\n",count);
}
void LoadSynchronizer::decrement(){
  int count = _load_counter.fetch_add(-1);
  printf("LoadSynchronizer::decrement count<%d>\n",count);
}
bool LoadSynchronizer::isComplete() const{
  return _load_counter.load()==0;
}

///////////////////////////////////////////////////////////////////////////////

LoadRequest::LoadRequest() { //
  _asset_vars = std::make_shared<vars_t>();
}

///////////////////////////////////////////////////////////////////////////////

LoadRequest::LoadRequest(datablock_ptr_t db,vars_ptr_t asset_vars) //
  : _asset_path("")
  , _asset_vars(asset_vars)
  , _datablock(db) { //
  if(_asset_vars==nullptr){
    _asset_vars = std::make_shared<vars_t>();
  }
}

///////////////////////////////////////////////////////////////////////////////

LoadRequest::LoadRequest(const AssetPath& p) //
  : _asset_path(p) { //
  _asset_vars = std::make_shared<vars_t>();
}

///////////////////////////////////////////////////////////////////////////////

LoadRequest::LoadRequest(const AssetPath& p,vars_ptr_t vars) //
  : _asset_path(p)
  , _asset_vars(vars) { //
}

///////////////////////////////////////////////////////////////////////////////

LoadRequest::LoadRequest(const AssetPath& p, catalog::assethandle_ptr_t catalog_handle)
  : _asset_path(p)
  , _catalog_handle(catalog_handle) {
  _asset_vars = std::make_shared<vars_t>();
  OrkAssert(catalog_handle != nullptr);
  OrkAssert(catalog_handle->isValid());
}

///////////////////////////////////////////////////////////////////////////////

LoadRequest::LoadRequest(catalog::assethandle_ptr_t catalog_handle)
  : _catalog_handle(catalog_handle) {
  _asset_vars = std::make_shared<vars_t>();
  OrkAssert(catalog_handle != nullptr);
  OrkAssert(catalog_handle->isValid());
  // Path will be resolved from catalog asset_id if needed
  if (!catalog_handle->_asset_id.empty()) {
    _asset_path = AssetPath(catalog_handle->_asset_id.c_str());
  }
}

void LoadRequest::enqueueAsync(void_lambda_t on_complete) const{

}

///////////////////////////////////////////////////////////////////////////////

void LoadRequest::incrementPartialLoadCount(){
  _partial_load_counter.fetch_add(1);
}

///////////////////////////////////////////////////////////////////////////////

void LoadRequest::decrementPartialLoadCount(){
  int this_count = _partial_load_counter.fetch_add(-1);
  if(this_count==1){
    if(_on_load_complete){
      _on_load_complete();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoadRequest::waitForCompletion() const { //
  bool done = false;
  while(not done){
    done = (_partial_load_counter.load()==0);
    if(not done){
      ork::usleep(100);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string LoadRequest::getAssetIdentifier() const {
  if (_catalog_handle) {
    // For catalog: namespace.assetid or namespace.derivedname
    if (!_catalog_handle->_asset_id.empty()) {
      return _catalog_handle->_namespace + "." + _catalog_handle->_asset_id;
    }
    // Derive from path - use base filename without extension
    std::string name = _asset_path.getName();
    size_t dot_pos = name.find_last_of('.');
    if (dot_pos != std::string::npos) {
      name = name.substr(0, dot_pos);
    }
    return _catalog_handle->_namespace + "." + name;
  }
  // For file: just the path
  return _asset_path.c_str();
}


///////////////////////////////////////////////////////////////////////////////

void Asset::describeX(object::ObjectClass* clazz) {
  clazz->template annotateTyped<ConstString>("editor.ged.node.factory", "GedNodeFactoryAssetList");
  clazz->directProperty("path", &Asset::_name);
}

Asset::Asset() {
}

///////////////////////////////////////////////////////////////////////////////

void Asset::setRequest(loadrequest_ptr_t req) {
  _load_request = req;
  _name = _load_request->_asset_path;
}

///////////////////////////////////////////////////////////////////////////////

AssetPath Asset::name() const {
  return _name;
}

///////////////////////////////////////////////////////////////////////////////

assetset_ptr_t Asset::assetset() const {
  auto objclazz  = rtti::safe_downcast<object::ObjectClass*>(GetClass());
  auto aset_anno = objclazz->annotation("AssetSet");
  auto asset_set = aset_anno.get<assetset_ptr_t>();
  return asset_set;
}

///////////////////////////////////////////////////////////////////////////////

std::string Asset::type() const {
  auto objclazz = rtti::safe_downcast<object::ObjectClass*>(GetClass());
  return objclazz->Name().c_str();
}

///////////////////////////////////////////////////////////////////////////////

bool Asset::Load() const {
  auto entry     = assetSetEntry(this);
  auto asset_set = assetset();
  return entry->Load(asset_set->topLevel());
}

bool Asset::LoadUnManaged() const {
  AssetSetEntry* entry = assetSetEntry(this);
  auto asset_set       = assetset();
  return entry->Load(asset_set->topLevel());
}

///////////////////////////////////////////////////////////////////////////////

bool Asset::IsLoaded() const {
  AssetSetEntry* entry = assetSetEntry(this);

  return entry && entry->IsLoaded();
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
