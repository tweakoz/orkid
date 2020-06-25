////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <ork/object/Object.h>
#include <ork/util/crc64.h>
#include <ork/util/Context.h>
#include <ork/kernel/tempstring.h>
#include <ork/rtti/RTTIX.inl>

namespace ork {

///////////////////////////////////////////////////////////////////////////

class HotKey : public ork::Object {
  RttiDeclareConcrete(HotKey, ork::Object);

public:
  int miKeyCode;
  bool mbAlt;
  bool mbCtrl;
  bool mbShift;
  bool mbLeftMB;
  bool mbRightMB;
  bool mbMiddleMB;

  boost::Crc64 GetHash() const;

  HotKey();
  HotKey(const char* keycode);

  FixedString<32> GetAcceleratorCode() const;
};

///////////////////////////////////////////////////////////////////////////

class HotKeyConfiguration : public ork::Object {
  DeclareConcreteX(HotKeyConfiguration, ork::Object);

  orklut<PoolString, ork::object_ptr_t> _hotkeys;
  orkset<boost::Crc64> mHotKeysUsed;

  bool PostDeserialize(reflect::IDeserializer&) final;

public:
  HotKeyConfiguration();

  void Default();

  void AddHotKey(const char* actionname, const HotKey& hkey);
  void RemoveHotKey(const char* actionname);
  void RemoveHotKey(const HotKey& hkey);
  bool IsHotKeyPresent(const HotKey& hkey) const;
  HotKey* GetHotKey(PoolString ps) const;
};

///////////////////////////////////////////////////////////////////////////

class HotKeyManager : public ork::Object {
  RttiDeclareAbstract(HotKeyManager, ork::Object);

  orklut<PoolString, ork::Object*> mHotKeyConfigurations;

  HotKeyConfiguration* mCurrent;

  HotKeyManager();

  bool PreDeserialize(reflect::IDeserializer&) final;
  bool PostDeserialize(reflect::IDeserializer&) final;

public:
  static HotKeyManager gHotKeyManager;

  static HotKeyManager& GetRef() {
    return gHotKeyManager;
  }

  void AddHotKeyConfiguration(const char* configname, const HotKeyConfiguration& HotKeyConfiguration);
  void RemoveHotKeyConfiguration(const char* actionname);
  HotKeyConfiguration* GetConfiguration(const char* actionname);

  void SetCurrentConfiguration(const char* configname);

  static const HotKey& GetHotKey(const char* actionname);

  void Save();
  void Load();

  static bool IsDepressed(PoolString action);
  static bool IsDepressed(const char* action);
  static bool IsDepressed(const HotKey& action);

  static FixedString<32> GetAcceleratorCode(const char* action);
};

///////////////////////////////////////////////////////////////////////////
} // namespace ork
