////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/asset/Asset.h>
#include <ork/asset/AssetSet.h>
#include <ork/asset/AssetSetLevel.h>
#include <ork/asset/AssetSetEntry.h>
#include <ork/asset/AssetLoader.h>
#include <ork/reflect/properties/register.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::asset {
///////////////////////////////////////////////////////////////////////////////

// class Asset;

template <typename Operator> //
static void Apply(
    AssetSetLevel* top_level, //
    Operator op,
    int depth = -1);

std::pair<AssetSetEntry*, bool> //
FindAssetEntryInternal(
    AssetSetLevel* top_level, //
    AssetPath name);

///////////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename ReturnType> //
class SimpleExecutor {
  ReturnType (ClassType::*mMemberFunction)(AssetSetLevel* level);
  AssetSetLevel* mLevel;

public:
  SimpleExecutor(ReturnType (ClassType::*function)(AssetSetLevel* level), AssetSetLevel* level)
      : mMemberFunction(function)
      , mLevel(level) {
  }

  ReturnType operator()(ClassType* object) {
    return (object->*mMemberFunction)(mLevel);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename ReturnType>
SimpleExecutor<ClassType, ReturnType> //
BuildExecutor(
    ReturnType (ClassType::*function)(AssetSetLevel*), //
    AssetSetLevel* level) {
  return SimpleExecutor<ClassType, ReturnType>(function, level);
}

///////////////////////////////////////////////////////////////////////////////

AssetSet::AssetSet()
    : mTopLevel(NULL) {
}

///////////////////////////////////////////////////////////////////////////////

void AssetSet::Register(
    AssetPath name, //
    asset_ptr_t asset,
    AssetLoader* loader) {
  if (NULL == mTopLevel)
    pushLevel(ork::rtti::safe_downcast<object::ObjectClass*>(asset->GetClass()));

  std::pair<AssetSetEntry*, bool> result = FindAssetEntryInternal(mTopLevel, name);
  AssetSetEntry* entry                   = result.first;

  if (entry == NULL) {
    OrkAssert(asset != NULL);
    entry = new AssetSetEntry(asset, loader, mTopLevel);
  }

  if (false == result.second)
    mTopLevel->Getset().push_back(entry);
}

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t AssetSet::FindAsset(AssetPath name) {
  auto entry = FindAssetEntry(name);

  if (entry)
    return entry->asset();

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////

AssetSetEntry* AssetSet::FindAssetEntry(AssetPath name) {
  return FindAssetEntryInternal(mTopLevel, name).first;
}

///////////////////////////////////////////////////////////////////////////////

AssetLoader* AssetSet::FindLoader(AssetPath name) {
  AssetSetEntry* entry = FindAssetEntry(name);

  if (entry)
    return entry->GetLoader();

  return NULL;
}

///////////////////////////////////////////////////////////////////////////////

std::pair<AssetSetEntry*, bool> FindAssetEntryInternal(AssetSetLevel* top_level, AssetPath name) {
  for (AssetSetLevel* level = top_level; //
       level != nullptr;
       level = level->Parent()) {
    auto levset = level->Getset();
    auto it     = std::find_if(
        levset.begin(), //
        levset.end(),
        [name](const AssetSetEntry* entry) -> bool { //
          return name == entry->asset()->name();
        });
    if (it != levset.end())
      return std::make_pair(*it, level == top_level);
  }

  return std::make_pair(static_cast<AssetSetEntry*>(NULL), false);
}

///////////////////////////////////////////////////////////////////////////////

bool AssetSet::Load(int depth) {
  int load_count = 0;
  ork::ConstString name("");

  for (AssetSetLevel* level = mTopLevel; //
       depth != 0 && level != NULL;
       level = level->Parent(), depth--) {
    for (orkvector<AssetSetEntry*>::size_type i = 0; i < level->Getset().size(); ++i) {
      AssetSetEntry* entry = level->Getset()[i];
      if (false == entry->IsLoaded()) {
        if (entry->Load(mTopLevel)) {
          load_count++;
          name = entry->asset()->GetClass()->Name();
        }
      }
    }
  }

  return load_count != 0;
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ORKCONFIG_ASSET_UNLOAD)
bool AssetSet::unload(int depth) {
  int unload_count = 0;
  ork::ConstString name("");

  for (AssetSetLevel* level = mTopLevel; depth != 0 && level != NULL; level = level->Parent(), depth--) {
    for (orkvector<AssetSetEntry*>::size_type i = 0; i < level->Getset().size(); ++i) {
      AssetSetEntry* entry = level->Getset()[i];
      if (entry->unload(mTopLevel)) {
        unload_count--;
        name = entry->asset()->GetClass()->Name();
      }
    }
  }

  return unload_count != 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////

void AssetSet::pushLevel(object::ObjectClass* type) {
  Apply(mTopLevel, BuildExecutor(&AssetSetEntry::OnPush, mTopLevel));

  mTopLevel = new AssetSetLevel(mTopLevel);
}

///////////////////////////////////////////////////////////////////////////////

void AssetSet::popLevel() {
  Apply(mTopLevel, BuildExecutor(&AssetSetEntry::OnPop, mTopLevel));

  AssetSetLevel* top_level = mTopLevel;

  mTopLevel = mTopLevel->Parent();

  top_level->~AssetSetLevel();
}

///////////////////////////////////////////////////////////////////////////////

AssetSetLevel* AssetSet::topLevel() const {
  return mTopLevel;
}

///////////////////////////////////////////////////////////////////////////////

template <typename Operator> void Apply(AssetSetLevel* top_level, Operator op, int depth) {
  for (AssetSetLevel* level = top_level; depth != 0 && level != NULL; level = level->Parent(), depth--) {
    std::for_each(level->Getset().begin(), level->Getset().end(), op);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
