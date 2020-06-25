///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////

#include <queue>

#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/properties/I.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectMapTyped.hpp>

#include <ork/util/crc.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::PersistantMap, "GedPersistantMap" );
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::PersistMapContainer, "GedPersistMapContainer" );

template class ork::orklut<int,ork::tool::ged::PersistantMap*>;

namespace ork::tool::ged {

  ///////////////////////////////////////////////////////////////////////////////

  void PersistMapContainer::Describe(){
  	ork::reflect::RegisterMapProperty( "Maps", & PersistMapContainer::mPropPersistMap );
  }

  ///////////////////////////////////////////////////////////////////////////////

  PersistMapContainer::PersistMapContainer(){}
  PersistMapContainer::~PersistMapContainer(){}

  ///////////////////////////////////////////////////////////////////////////////

  void PersistMapContainer::CloneFrom( const PersistMapContainer& oth ){
  	for( auto item : oth.mPropPersistMap ) {
  		ork::Object* pclone = item.second->Clone();
  		PersistantMap* pclone_map = rtti::autocast(pclone);
  		mPropPersistMap.AddSorted( item.first, pclone_map );
  	}
  }

  ///////////////////////////////////////////////////////////////////////////////

  void PersistantMap::Describe(){
  	ork::reflect::RegisterMapProperty( "CollapeState", & PersistantMap::mProperties );
  }

  ///////////////////////////////////////////////////////////////////////////////

  PersistantMap::PersistantMap(){}
  PersistantMap::~PersistantMap(){}

  ///////////////////////////////////////////////////////////////////////////////

  PersistHashContext::PersistHashContext()
  	: mProperty(0)
  	, mObject(0)
  	, mString(0)
  {
  }

  ///////////////////////////////////////////////////////////////////////////////

  int PersistHashContext::GenerateHash() const {
  	U32 phash = 0;
  	U32 ohash = 0;
  	const char* classname = 0;
  	if( mProperty ){
  		ork::rtti::Class* pclass = mProperty->GetClass();
  		const ork::PoolString & name = pclass->Name();
  		const char* pname = name.c_str();
  		phash = Crc32::HashMemory( pname, int(strlen(pname)));
  	}
  	if( mObject ){
  		ork::rtti::Class* pclass = mObject->GetClass();
  		const ork::PoolString & name = pclass->Name();
  		ohash = Crc32::HashMemory( name.c_str(), int(strlen(name.c_str())));
  	}
  	U32 key = phash^ohash;
  	int ikey = *reinterpret_cast<int*>(&key);
  	return ikey;
  }

  ///////////////////////////////////////////////////////////////////////////////

  PersistantMap* ObjModel::GetPersistMap( const PersistHashContext& Ctx ) {
  	PersistantMap* prval = 0;
  	int key = Ctx.GenerateHash();
  	orklut<int, PersistantMap* >::const_iterator it = mPersistMapContainer.GetMap().find( key );
  	if( it== mPersistMapContainer.GetMap().end() ) {
  		prval = new PersistantMap;
  		mPersistMapContainer.GetMap().AddSorted( key , prval );
  	}
  	else
  		prval =  it->second;
  	return prval;
  }

  ///////////////////////////////////////////////////////////////////////////////

  const std::string& PersistantMap::GetValue( const std::string& key ) {
  	orklut<std::string,std::string>::const_iterator it = mProperties.find( key );
  	if( it == mProperties.end() ) {
  		mProperties.AddSorted(key,"");
  		it = mProperties.find( key );
  	}
  	return it->second;
  }

  ///////////////////////////////////////////////////////////////////////////////

  void PersistantMap::SetValue( const std::string& key, const std::string& val ) {
  	orklut<std::string,std::string>::iterator it = mProperties.find( key );
  	if( it == mProperties.end() )
  		it = mProperties.AddSorted(key,val);
  	else
  		it->second = val;
  }
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::tool::ged {
