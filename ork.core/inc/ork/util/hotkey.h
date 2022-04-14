////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
  DeclareConcreteX(HotKey, ork::Object);

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

using hotkey_ptr_t      = std::shared_ptr<HotKey>;
using hotkey_constptr_t = std::shared_ptr<const HotKey>;

///////////////////////////////////////////////////////////////////////////

class HotKeyConfiguration : public ork::Object {
  DeclareConcreteX(HotKeyConfiguration, ork::Object);

  bool postDeserialize(reflect::serdes::IDeserializer&) final;

public:
  HotKeyConfiguration();

  void Default();

  void AddHotKey(const char* actionname, const HotKey& hkey);
  void RemoveHotKey(const char* actionname);
  void RemoveHotKey(const HotKey& hkey);
  bool IsHotKeyPresent(const HotKey& hkey) const;
  HotKey* GetHotKey(std::string ps) const;

  orklut<std::string, hotkey_ptr_t> _hotkeys;
  orkset<boost::Crc64> mHotKeysUsed;
};

using hotkeyconfig_ptr_t      = std::shared_ptr<HotKeyConfiguration>;
using hotkeyconfig_constptr_t = std::shared_ptr<const HotKeyConfiguration>;

///////////////////////////////////////////////////////////////////////////

class HotKeyManager : public ork::Object {
  RttiDeclareAbstract(HotKeyManager, ork::Object);

  orklut<PoolString, ork::object_ptr_t> _configurations;

  hotkeyconfig_ptr_t _currentConfiguration;

  HotKeyManager();

public:
  void AddHotKeyConfiguration(const char* configname, const HotKeyConfiguration& HotKeyConfiguration);
  void RemoveHotKeyConfiguration(const char* actionname);
  HotKeyConfiguration* GetConfiguration(const char* actionname);

  void SetCurrentConfiguration(const char* configname);

  static const HotKey& GetHotKey(const char* actionname);

  static void Save(std::shared_ptr<HotKeyManager> hkm);
  static std::shared_ptr<HotKeyManager> Load(std::string path);

  // static bool IsDepressed(const char* action);
  // static bool IsDepressed(const HotKey& action);
  // static FixedString<32> GetAcceleratorCode(const char* action);
};

///////////////////////////////////////////////////////////////////////////
} // namespace ork
