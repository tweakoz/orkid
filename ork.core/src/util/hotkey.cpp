////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/util/hotkey.h>
#include <ork/util/Context.hpp>
#include <ork/kernel/string/PoolString.h>
#include <ork/application/application.h>
#include <ork/file/path.h>
#include <ork/file/fileenv.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>

#include <ork/reflect/RegisterProperty.h>



INSTANTIATE_TRANSPARENT_RTTI( ork::HotKey, "HotKey" );
INSTANTIATE_TRANSPARENT_RTTI( ork::HotKeyConfiguration, "HotKeyConfiguration" );
INSTANTIATE_TRANSPARENT_RTTI( ork::HotKeyManager, "HotKeyManager" );

namespace ork {

HotMouseTest::HotMouseTest( bool lmb, bool mmb, bool rmb, bool alt, bool shf, bool ctl )
	: mbCurLMB(lmb)
	, mbCurMMB(mmb)
	, mbCurRMB(rmb)
	, mbCurAlt(alt)
	, mbCurShf(shf)
	, mbCurCtl(ctl)
{

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void HotKey::Describe()
{
	ork::reflect::RegisterProperty("KeyCode", &HotKey::miKeyCode);
	ork::reflect::RegisterProperty("Alt", &HotKey::mbAlt);
	ork::reflect::RegisterProperty("Ctrl", &HotKey::mbCtrl);
	ork::reflect::RegisterProperty("Shift", &HotKey::mbShift);
	ork::reflect::RegisterProperty("LMB", &HotKey::mbLeftMB);
	ork::reflect::RegisterProperty("MMB", &HotKey::mbMiddleMB);
	ork::reflect::RegisterProperty("RMB", &HotKey::mbRightMB);
}

///////////////////////////////////////////////////////////////////////////////

HotKey::HotKey()
	: miKeyCode(-1)
	, mbAlt( false )
	, mbCtrl( false )
	, mbShift( false )
	, mbLeftMB( false )
	, mbMiddleMB( false )
	, mbRightMB( false )
{
}

///////////////////////////////////////////////////////////////////////////////

HotKey::HotKey( const char* keycode )
	: miKeyCode(-1)
	, mbAlt( false )
	, mbCtrl( false )
	, mbShift( false )
	, mbLeftMB( false )
	, mbMiddleMB( false )
	, mbRightMB( false )
{

	mbAlt = strstr(keycode,"alt")!=0;
	mbCtrl = strstr(keycode,"ctrl")!=0;
	mbShift = strstr(keycode,"shift")!=0;
	mbLeftMB = strstr(keycode,"lmb")!=0;
	mbMiddleMB = strstr(keycode,"mmb")!=0;
	mbRightMB = strstr(keycode,"rmb")!=0;

	const char* plast=strrchr( keycode, '-' );

	const char* kcstr = (plast==0) ? keycode : plast+1;

	size_t ilen = strlen(kcstr);

	switch( ilen )
	{
		case 1:
		{
			char ch = kcstr[0];

			#if 0 //defined(IX)
			if( ch>='f' )
			{
				miKeyCode = 3;
			}
			else if( ch>='a' && ch<='z' )
			{
				miKeyCode = int(ch-'a')+'A';
			}

			#else
			if( ch>='a' && ch<='z' )
			{
				miKeyCode = int(ch-'a')+'A';
			}
			else if( ch>='A' && ch<='Z' )
			{
				miKeyCode = int(ch-'A')+'A';
			}
			else if( ch>='0' && ch<='9' )
			{
				miKeyCode = 26+int(ch-'0');
			}
			else switch( ch )
			{
				case '[':
				case ']':
				case '\\':
				case '-':
				case '=':
				case ';':
				case '\'':
				case '/':
				case ',':
				case '.':
				case '`':
					miKeyCode = int(ch);
					break;
			}
			#endif
			break;
		}
		case 2: // f keys
		case 3:
		{	//

			break;
		}
		case 4: // numeric keypad
		{
			const char* pnum=strstr( keycode, "num" );
			if( pnum )
			{
				int idigit = keycode[3]-'0';
				#if defined(_WIN32)
				miKeyCode = VK_NUMPAD0+idigit;
				#endif
			}
			else if( strstr( keycode, "none" )!=0 )
			{
				miKeyCode=-2;
			}

			break;
		}
	}


}

FixedString<32> HotKey::GetAcceleratorCode() const
{
	FixedString<32> rval;
	FixedString<32> rv_c;
	FixedString<32> rv_a;
	FixedString<32> rv_s;
	FixedString<32> rv_ch;

	if( mbCtrl )
	{
		rv_c.set( "Ctrl" );
	}
	if( mbShift )
	{
		if( mbCtrl ) rv_s.set( "+Shift" ); else rv_s.set( "Shift" );
	}
	if( mbAlt )
	{
		if( mbCtrl||mbShift ) rv_a.set( "+Alt" ); else rv_a.set( "Alt" );
	}
	{
		if( mbCtrl||mbShift||mbAlt ) rv_ch.format( "+%c",char(miKeyCode) ); else rv_ch.format( "%c",char(miKeyCode) );
	}
	rval.format( "%s%s%s%s", rv_c.c_str(), rv_s.c_str(), rv_a.c_str(),rv_ch.c_str() );

	return rval;

}

///////////////////////////////////////////////////////////////////////////////

boost::Crc64 HotKey::GetHash() const
{
	boost::Crc64 rval;
	boost::crc64_init(rval);
	boost::crc64_compute( rval, & miKeyCode, sizeof( miKeyCode ) );
	boost::crc64_compute( rval, & mbAlt, sizeof( mbAlt ) );
	boost::crc64_compute( rval, & mbCtrl, sizeof( mbCtrl ) );
	boost::crc64_compute( rval, & mbShift, sizeof( mbShift ) );
	boost::crc64_compute( rval, & mbLeftMB, sizeof( mbLeftMB ) );
	boost::crc64_compute( rval, & mbMiddleMB, sizeof( mbMiddleMB ) );
	boost::crc64_compute( rval, & mbRightMB, sizeof( mbRightMB ) );
	boost::crc64_fin(rval);
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void HotKeyConfiguration::Describe()
{
	ork::reflect::RegisterMapProperty("HotKeys", &HotKeyConfiguration::mHotKeys);
	ork::reflect::AnnotatePropertyForEditor< HotKeyConfiguration >("HotKeys", "editor.factorylistbase", "HotKey" );

}

///////////////////////////////////////////////////////////////////////////////

HotKeyConfiguration::HotKeyConfiguration()
{
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyConfiguration::Default()
{
	AddHotKey( "copy", "ctrl-c" );
	AddHotKey( "paste", "ctrl-v" );
	AddHotKey( "open", "ctrl-o" );
	AddHotKey( "save", "ctrl-s" );
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyConfiguration::AddHotKey( const char* actionname, const HotKey& hkey )
{
	ork::PoolString psname = ork::AddPooledString( actionname );
	if( IsHotKeyPresent( hkey ) )
	{
	}
	else
	{
		mHotKeys.AddSorted( psname, new HotKey( hkey ) );
		mHotKeysUsed.insert(hkey.GetHash());
	}
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyConfiguration::RemoveHotKey(  const char* actionname )
{
	ork::PoolString psname = ork::AddPooledString( actionname );
	//if( IsHotKeyPresent( hkey ) )
	{
	//	mHotKeysUsed.erase(hkey.GetHash());
	}
}

bool HotKeyConfiguration::PostDeserialize(reflect::IDeserializer &) // virtual
{
	for( orklut<PoolString,ork::Object*>::const_iterator it=mHotKeys.begin(); it!=mHotKeys.end(); it++ )
	{
		ork::Object* pobj = it->second;
		HotKey* pkey = rtti::autocast( pobj );
		OrkAssert( mHotKeysUsed.find( pkey->GetHash() ) == mHotKeysUsed.end() );
		mHotKeysUsed.insert( pkey->GetHash() );
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyConfiguration::RemoveHotKey( const HotKey& hkey )
{
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyConfiguration::IsHotKeyPresent( const HotKey& hkey ) const
{
	boost::Crc64 hash = hkey.GetHash();
	return (mHotKeysUsed.find(hash) != mHotKeysUsed.end());
}

HotKey* HotKeyConfiguration::GetHotKey( PoolString ps ) const
{	orklut<PoolString,ork::Object*>::const_iterator it = mHotKeys.find( ps );
	if( it != mHotKeys.end() )
	{	HotKey* pkey = rtti::autocast( it->second );
		return pkey;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

HotKeyManager HotKeyManager::gHotKeyManager;

void HotKeyManager::Describe()
{
	ork::reflect::RegisterMapProperty("Configurations", &HotKeyManager::mHotKeyConfigurations);
	ork::reflect::AnnotatePropertyForEditor< HotKeyManager >("Configurations", "editor.factorylistbase", "HotKeyConfiguration" );
}

///////////////////////////////////////////////////////////////////////////////

HotKeyManager::HotKeyManager()
{
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyManager::AddHotKeyConfiguration( const char* configname, const HotKeyConfiguration& hkc )
{	ork::PoolString psname = ork::AddPooledString( configname );
	orklut<PoolString,ork::Object*>::const_iterator it=mHotKeyConfigurations.find(psname);
	if( it==mHotKeyConfigurations.end() )
	{	HotKeyConfiguration* dupe = new HotKeyConfiguration(hkc);
		mHotKeyConfigurations.AddSorted( psname, dupe );
		mCurrent = dupe;
	}
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyManager::RemoveHotKeyConfiguration( const char* actionname )
{
}

///////////////////////////////////////////////////////////////////////////////

HotKeyConfiguration* HotKeyManager::GetConfiguration( const char* actionname )
{
	return mCurrent;
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyManager::SetCurrentConfiguration( const char* configname )
{	ork::PoolString psname = ork::AddPooledString( configname );
	orklut<PoolString,ork::Object*>::const_iterator it=mHotKeyConfigurations.find(psname);
	if( it!=mHotKeyConfigurations.end() )
	{	mCurrent = rtti::autocast( it->second );
	}
}

///////////////////////////////////////////////////////////////////////////////

const HotKey& HotKeyManager::GetHotKey( const char* actionname )
{	ork::PoolString psname = ork::AddPooledString( actionname );
	if( GetRef().mCurrent )
	{	HotKey* hkey = GetRef().mCurrent->GetHotKey( psname );
		if( hkey )
		{
			return *hkey;
		}
	}
	static const HotKey gnull;
	return gnull;
}

///////////////////////////////////////////////////////////////////////////////

static const char* HotKeyFileName = "hotkeys.ork";

void HotKeyManager::Save()
{	ork::stream::FileOutputStream istream(HotKeyFileName);
	ork::reflect::serialize::XMLSerializer ser(istream);
	GetClass()->Description().SerializeProperties(ser, this);
}

///////////////////////////////////////////////////////////////////////////////

void HotKeyManager::Load()
{	mHotKeyConfigurations.clear();
	file::Path pth(HotKeyFileName);
	if( ork::CFileEnv::GetRef().DoesFileExist( pth ) )
	{	ork::stream::FileInputStream istream(pth.c_str());
		ork::reflect::serialize::XMLDeserializer deser(istream);
		GetClass()->Description().DeserializeProperties(deser, this);
	}
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyManager::IsDepressed( const HotKey& action, const HotMouseTest& hms )
{	OrkAssert( action.miKeyCode < 0 );
	bool blmb = action.mbLeftMB ? hms.mbCurLMB : true;
	bool bmmb = action.mbMiddleMB ? hms.mbCurMMB : true;
	bool brmb = action.mbRightMB ? hms.mbCurRMB : true;
	bool balt = action.mbAlt ? hms.mbCurAlt : true;
	bool bshf = action.mbShift ? hms.mbCurShf : true;
	bool bctl = action.mbCtrl ? hms.mbCurCtl : true;
	return (blmb&&bmmb&&brmb&&balt&&bshf&&bctl);
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyManager::IsDepressed( const HotKey& hkey )
{	if( GetRef().mCurrent )
	{	int ikc = hkey.miKeyCode;
		if( ikc>=0 )
		{
#if defined(IX)
			return CSystem::IsKeyDepressed(ikc);
#elif defined(_XBOX)
			return false;
#elif defined(_WIN32)

			bool bks = (GetAsyncKeyState(ikc) & 0x8000) == 0x8000;
			bool bkalt = hkey.mbAlt?(GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0x8000:true;
			bool bkshf = hkey.mbShift?(GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0x8000:true;
			bool bkctl = hkey.mbCtrl?(GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0x8000:true;
			bool bmodok		= bkalt&&bkshf&&bkctl;
			return (bmodok&&bks);
#endif
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyManager::IsDepressed( PoolString pact )
{	if( GetRef().mCurrent )
	{	HotKey* hkey = GetRef().mCurrent->GetHotKey( pact );
		if( hkey )
		{
			return IsDepressed( *hkey );
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyManager::IsDepressed( const char* pact )
{
	//if( 0 == strcmp( pact, "camera_bwd" ) )
	//{
	//	int i = 0;
	//}
	//
	return IsDepressed( ork::AddPooledString( pact ) );
}

///////////////////////////////////////////////////////////////////////////////

FixedString<32> HotKeyManager::GetAcceleratorCode(const char* action)
{
	FixedString<32> rval;

	ork::PoolString psname = ork::AddPooledString( action );
	if( GetRef().mCurrent )
	{	HotKey* hkey = GetRef().mCurrent->GetHotKey( psname );
		if( hkey )
		{
			rval = hkey->GetAcceleratorCode();
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyManager::PreDeserialize(reflect::IDeserializer &) // virtual
{
	mHotKeyConfigurations.clear();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool HotKeyManager::PostDeserialize(reflect::IDeserializer &) // virtual
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////

}

