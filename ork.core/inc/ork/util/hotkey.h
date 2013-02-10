////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_UTIL_HOTKEY_H
#define _ORK_UTIL_HOTKEY_H

#include <ork/pch.h>
#include <ork/object/Object.h>
#include <ork/util/crc64.h>
#include <ork/util/Context.h>
#include <ork/kernel/tempstring.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////

struct HotMouseTest
{
	bool	mbCurLMB;
	bool	mbCurMMB;
	bool	mbCurRMB;
	bool	mbCurAlt;
	bool	mbCurShf;
	bool	mbCurCtl;

	HotMouseTest( bool lmb, bool mmb, bool rmb, bool alt, bool shf, bool ctl );
};

///////////////////////////////////////////////////////////////////////////

class HotKey : public ork::Object
{
	RttiDeclareConcrete(HotKey,ork::Object);

public:

	int				miKeyCode;
	bool			mbAlt;
	bool			mbCtrl;
	bool			mbShift;
	bool			mbLeftMB;
	bool			mbRightMB;
	bool			mbMiddleMB;

	boost::Crc64	GetHash() const;

	HotKey();
	HotKey( const char* keycode );

	FixedString<32> GetAcceleratorCode() const ;
};

///////////////////////////////////////////////////////////////////////////

class HotKeyConfiguration : public ork::Object
{
	RttiDeclareConcrete(HotKeyConfiguration,ork::Object);

	orklut<PoolString,ork::Object*>	mHotKeys;
	orkset<boost::Crc64>			mHotKeysUsed;

	bool PostDeserialize(reflect::IDeserializer &); // virtual 

public:

	HotKeyConfiguration();

	void Default();

	void AddHotKey( const char* actionname, const HotKey& hkey );
	void RemoveHotKey( const char* actionname );
	void RemoveHotKey( const HotKey& hkey );
	bool IsHotKeyPresent(const HotKey& hkey) const;
	HotKey* GetHotKey( PoolString ps ) const ;

};

///////////////////////////////////////////////////////////////////////////

class HotKeyManager : public ork::Object
{
	RttiDeclareAbstract(HotKeyManager,ork::Object);

	orklut<PoolString,ork::Object*>	mHotKeyConfigurations;

	HotKeyConfiguration*	mCurrent;

	HotKeyManager();

public:

	static HotKeyManager gHotKeyManager;

	static HotKeyManager& GetRef() { return gHotKeyManager; }

	void AddHotKeyConfiguration( const char* configname, const HotKeyConfiguration& HotKeyConfiguration );
	void RemoveHotKeyConfiguration( const char* actionname );
	HotKeyConfiguration* GetConfiguration( const char* actionname );

	void SetCurrentConfiguration( const char* configname );

	static const HotKey& GetHotKey( const char* actionname );


	void Save();
	void Load();

	bool PreDeserialize(reflect::IDeserializer &); // virtual 
	bool PostDeserialize(reflect::IDeserializer &); // virtual 

	static bool IsDepressed(PoolString action);
	static bool IsDepressed(const char* action);
	static bool IsDepressed(const HotKey& action);

	static bool IsDepressed( const HotKey& action, const HotMouseTest& hms );

	static FixedString<32> GetAcceleratorCode(const char* action) ;
};

///////////////////////////////////////////////////////////////////////////
}

#endif
