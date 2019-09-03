////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/AutoConnector.h>
#include <ork/rtti/Class.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/rtti/downcast.h>

#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::Object, "Object");
INSTANTIATE_TRANSPARENT_RTTI(ork::AutoConnector, "AutoConnector");

namespace ork {

void Object::Describe()
{
}
bool Object::Serialize(reflect::ISerializer &serializer) const
{
	bool result = true;
	rtti::Class *clazz = this->GetClass();

	reflect::Command command(reflect::Command::EOBJECT, clazz->Name());

	if(false == serializer.BeginCommand(command))
		result = false;
	if(false == serializer.ReferenceObject(this))
		result = false;

	if(false == this->PreSerialize(serializer))
		result = false;

	if(false == rtti::safe_downcast<object::ObjectClass *>(clazz)->Description().SerializeProperties(serializer, this))
		result = false;

	if(false == this->PostSerialize(serializer))
		result = false;

	if(false == serializer.EndCommand(command))
		result = false;

	return result;
}

bool Object::SerializeInPlace(reflect::ISerializer &serializer) const
{
	bool result = true;
	rtti::Class *clazz = this->GetClass();

	reflect::Command command(reflect::Command::EOBJECT, clazz->Name());

	if(false == serializer.BeginCommand(command))
		result = false;
	//if(false == serializer.ReferenceObject(this))
		//result = false;

	if(false == this->PreSerialize(serializer))
		result = false;

	if(false == rtti::safe_downcast<object::ObjectClass *>(clazz)->Description().SerializeProperties(serializer, this))
		result = false;

	if(false == this->PostSerialize(serializer))
		result = false;

	if(false == serializer.EndCommand(command))
		result = false;

	return result;
}

bool Object::Deserialize(reflect::IDeserializer &deserializer)
{
	bool result = true;
	rtti::Class *clazz = this->GetClass();

	deserializer.ReferenceObject(this);

	if(result) result = this->PreDeserialize(deserializer);

	if(false == rtti::safe_downcast<object::ObjectClass *>(clazz)->Description().DeserializeProperties(deserializer, this))
		result = false;

	if(result) result = this->PostDeserialize(deserializer);

	return result;
}

bool Object::DeserializeInPlace(reflect::IDeserializer &deserializer)
{
	bool result = true;
	rtti::Class *clazz = this->GetClass();

	reflect::Command command(reflect::Command::EOBJECT, clazz->Name());
	if(false == deserializer.BeginCommand(command))
		result = false;

	//deserializer.ReferenceObject(this);

	if(result) result = this->PreDeserialize(deserializer);

	if(false == rtti::safe_downcast<object::ObjectClass *>(clazz)->Description().DeserializeProperties(deserializer, this))
		result = false;

	if(result) result = this->PostDeserialize(deserializer);

	if(false == deserializer.EndCommand(command))
		result = false;

	return result;
}

object::Signal *Object::FindSignal(ConstString name)
{
	object::Signal Object::*pSignal = rtti::downcast<object::ObjectClass*>(GetClass())->Description().FindSignal(name);

	if(pSignal != 0)
		return &(this->*pSignal);
	else
		return NULL;
}

bool Object::PreSerialize(reflect::ISerializer &) const
{
	return true;
}

bool Object::PreDeserialize(reflect::IDeserializer &)
{
	return true;
}

bool Object::PostSerialize(reflect::ISerializer &) const
{
	return true;
}

bool Object::PostDeserialize(reflect::IDeserializer &)
{
	return true;
}

Object *Object::Clone() const
{
	printf( "slowclone class<%s>\n", GetClass()->Name().c_str() );

	if(Object *clone = rtti::autocast(GetClass()->CreateObject()))
	{
		ork::ResizableString str;
		ork::stream::ResizableStringOutputStream ostream(str);
		ork::reflect::serialize::BinarySerializer binoser(ostream);
		ork::reflect::serialize::ShallowSerializer oser(binoser);

		GetClass()->Description().SerializeProperties(oser, this);

		ork::stream::StringInputStream istream(str);
		ork::reflect::serialize::BinaryDeserializer biniser(istream);
		ork::reflect::serialize::ShallowDeserializer iser(biniser);

		GetClass()->Description().DeserializeProperties(iser, clone);

		return clone;
	}
	return NULL;
}

Md5Sum Object::CalcMd5() const
{
	ork::ResizableString str;
	ork::stream::ResizableStringOutputStream ostream(str);
	ork::reflect::serialize::BinarySerializer binoser(ostream);
	//ork::reflect::serialize::ShallowSerializer oser(binoser);
	GetClass()->Description().SerializeProperties(binoser, this);

	CMD5 md5_context;
	md5_context.update( (const uint8_t*) str.data(),str.length());
	md5_context.finalize();

	return md5_context.Result();
}


reflect::BidirectionalSerializer &operator ||(reflect::BidirectionalSerializer &bidi, Object &object)
{
	if(bidi.Serializing())
	{
		return bidi || static_cast<const Object &>(object);
	}
	else
	{
		reflect::IDeserializer &deserializer = *bidi.Deserializer();

		reflect::Command object_command;

		if(false == deserializer.BeginCommand(object_command))
			bidi.Fail();

		rtti::Class *clazz = rtti::Class::FindClass(object_command.Name());
		
		OrkAssertI(object.GetClass()->IsSubclassOf(clazz), "Can't deserialize an X into a Y");

		if(object.GetClass()->IsSubclassOf(clazz))
		{
			if(false == object.Deserialize(deserializer))
				bidi.Fail();
		}

		if(false == deserializer.EndCommand(object_command))
			bidi.Fail();
	}

	return bidi;
}

reflect::BidirectionalSerializer &operator ||(reflect::BidirectionalSerializer &bidi, const Object &object)
{
	OrkAssertI(bidi.Serializing(), "can't deserialize to a non-const object");

	if(bidi.Serializing())
	{
		if(false == object.Serialize(*bidi.Serializer()))
			bidi.Fail();
	}

	return bidi;
}

static Object *LoadObjectFromFile(ConstString filename, bool binary)
{
	float ftime1 = ork::OldSchool::GetRef().GetLoResRelTime();
	stream::FileInputStream stream(filename.c_str());

	Object *object = NULL;
	if(binary)
	{
		reflect::serialize::BinaryDeserializer deserializer(stream);

		DeserializeUnknownObject(deserializer, object);
	}
	else
	{
		reflect::serialize::XMLDeserializer deserializer(stream);

		DeserializeUnknownObject(deserializer, object);
	}

	float ftime2 = ork::OldSchool::GetRef().GetLoResRelTime();

	static float ftotaltime = 0.0f;
	static int iltotaltime = 0;

	ftotaltime += (ftime2-ftime1);

	int itotaltime = int(ftotaltime);

	//if( itotaltime > iltotaltime )
	{
		std::string outstr = ork::CreateFormattedString(
		"MOX AccumTime<%f>\n", ftotaltime );
		//OutputDebugString( outstr.c_str() );
		iltotaltime = itotaltime;
	}

	return object;
}

Object *DeserializeObject(PieceString file)
{
	ArrayString<256> filename_data = file;
	MutableString filename(filename_data);

	if(filename.substr(filename.length() - 4) == ".mox")
	{
		return LoadObjectFromFile(filename, false);
	}
	else if(filename.substr(filename.length() - 4) == ".mob")
	{
		return LoadObjectFromFile(filename, true);
	}
	else
	{
		filename = file;
		filename += ".mox";

		if(FileEnv::DoesFileExist(filename.c_str()))
		{
			return LoadObjectFromFile(filename, false);
		}

		filename = file;
		filename += ".mob";

		if(FileEnv::DoesFileExist(filename.c_str()))
		{
			return LoadObjectFromFile(filename, true);
		}
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AutoConnector::Describe()
{
}

AutoConnector::AutoConnector()
{
}
AutoConnector::~AutoConnector()
{
}
void AutoConnector::DisconnectAll()
{
	int inumcon = mConnections.size();

	while( false == mConnections.empty() )
	{
		Connection* conn = *mConnections.begin();

		bool bOK = ork::object::Disconnect( conn->mpSender, conn->mSignal, conn->mpReciever, conn->mSlot );
		OrkAssert(bOK);

		////////////////////////////////////////////////////
		// remove from my connection list
		////////////////////////////////////////////////////
		mConnections.erase(mConnections.begin()); 
		////////////////////////////////////////////////////

		////////////////////////////////////////////////////
		// remove from recievers connection list
		////////////////////////////////////////////////////
		orkset<Connection*>::iterator itoth;
		if( this == conn->mpSender )
		{
			itoth = conn->mpReciever->mConnections.find(conn);
			if( itoth != conn->mpReciever->mConnections.end() )
			{
				mConnections.erase(itoth); // remove from other connection list
			}
		}
		else if( this == conn->mpReciever )
		{
			itoth = conn->mpSender->mConnections.find(conn);
			if( itoth != conn->mpSender->mConnections.end() )
			{
				mConnections.erase(itoth); // remove from other connection list
			}
		}
		////////////////////////////////////////////////////

		delete conn;
	}
}

void AutoConnector::Connect( const char* SignalName, AutoConnector* pReciever, const char* SlotName )
{
	ork::PoolString psigname = ork::AddPooledString(SignalName);
	ork::PoolString psltname = ork::AddPooledString(SlotName);

	bool bOK = ork::object::Connect( this, psigname, pReciever, psltname );

	OrkAssert( bOK );

	if( bOK )
	{	Connection* conn = new Connection;
		conn->mpSender = this;
		conn->mpReciever = pReciever;
		conn->mSignal = psigname;
		conn->mSlot = psltname;
		mConnections.insert(conn);
		if( pReciever!=this )
		{
			pReciever->mConnections.insert(conn);
		}
	}
}

void AutoConnector::SetupSignalsAndSlots()
{
	object::ObjectClass* pclass = rtti::downcast<object::ObjectClass*>(GetClass());
	const reflect::Description& descript = pclass->Description();
	const reflect::Description::SignalMapType& signals = descript.GetSignals();
	const reflect::Description::AutoSlotMapType& autoslots = descript.GetAutoSlots();
	const reflect::Description::FunctorMapType& functors = descript.GetFunctors();

	for( reflect::Description::AutoSlotMapType::const_iterator it=autoslots.begin(); it!=autoslots.end(); it++ )
	{	const ork::ConstString& slotname = it->first;
		ork::object::AutoSlot ork::Object::* const ptr2slotmp = it->second;
		ork::object::AutoSlot& slot = this->*ptr2slotmp;
		slot.SetSlotName( ork::AddPooledString(slotname.c_str()) );
		slot.SetObject( this );
	}
	//for( reflect::Description::AutoSlotMapType::const_iterator it=autoslots.begin(); it!=autoslots.end(); it++ )
	//{	const ork::PoolString& slotname = it->first;
	//	AutoSlot* ptr2slot = it->second;
	//	ptr2slot->SetName( slotname );
	//	ptr2slot->SetObject( this );
	//}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}
