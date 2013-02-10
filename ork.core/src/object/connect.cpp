////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/connect.h>
#include <ork/application/application.h>
#include <ork/reflect/RegisterProperty.h>

#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>

#include <ork/object/ObjectClass.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::object::Slot, "Slot");
INSTANTIATE_TRANSPARENT_RTTI(ork::object::AutoSlot, "AutoSlot");
INSTANTIATE_TRANSPARENT_RTTI(ork::object::Signal, "Signal");

namespace ork { namespace reflect {
template<> void Serialize<ork::object::Signal>(ork::object::Signal const *in, ork::object::Signal *out, ::ork::reflect::BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi || in;
	}
	else
	{
		bidi || out;
	}
}
} }

namespace ork { namespace reflect {
template<> void Serialize<ork::object::AutoSlot>(ork::object::AutoSlot const *in, ork::object::AutoSlot *out, ::ork::reflect::BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi || in;
	}
	else
	{
		bidi || out;
	}
}
} }

namespace ork { namespace object {

//////////////////////////////////////////////////////////////////////

void Slot::Describe()
{
	reflect::RegisterProperty("Object", &Slot::mObject);
	reflect::RegisterProperty("SlotName", &Slot::mSlotName);
}
Slot::~Slot()
{
}
Slot::Slot(Object* object, PoolString name) : mObject(object), mSlotName(name)
{
}
Object* Slot::GetObject() const
{
	return mObject;
}

const PoolString& Slot::GetSlotName() const
{
	return mSlotName;
}

const reflect::IObjectFunctor* Slot::GetFunctor() const
{
	OrkAssert(mObject);
	return rtti::downcast<object::ObjectClass*>(mObject->GetClass())->Description().FindFunctor(mSlotName);
}

void Slot::Invoke(reflect::IInvokation* invokation) const
{
	GetFunctor()->invoke(mObject, invokation);
}

//////////////////////////////////////////////////////////////////////

void AutoSlot::Describe()
{
	//reflect::RegisterArrayProperty("Signals", &Signal::GetSlot, &Signal::GetSlotCount, &Signal::ResizeSlots);
}

AutoSlot::~AutoSlot()
{
	for( orkset<Signal*>::const_iterator it=mConnectedSignals.begin(); it!=mConnectedSignals.end(); it++ )
	{
		Signal* psig = (*it);
		psig->RemoveSlot(this);
	}
}

AutoSlot::AutoSlot(Object* object, const char* name) 
	: Slot( object, ork::AddPooledString(name) )
{
	OrkAssert( object != 0 );
}

void AutoSlot::AddSignal( Signal* psig )
{
	orkset<Signal*>::iterator it = mConnectedSignals.find( psig );
	if( it != mConnectedSignals.end() )
	{
		OrkAssert(false);
	}
	mConnectedSignals.insert( psig );
}
void AutoSlot::RemoveSignal( Signal* psig )
{
	orkset<Signal*>::iterator it = mConnectedSignals.find( psig );
	if( it == mConnectedSignals.end() )
	{
		OrkAssert(false);
	}
	mConnectedSignals.erase( psig );
}
//////////////////////////////////////////////////////////////////////

void Signal::Describe()
{
	reflect::RegisterArrayProperty("Slots", &Signal::GetSlot, &Signal::GetSlotCount, &Signal::ResizeSlots);
}

Signal::Signal()
{
}
Signal::~Signal()
{
	for( orkvector<Slot*>::const_iterator it=mSlots.begin(); it!=mSlots.end(); it++ )
	{
		Slot* pslot = (*it);
		pslot->RemoveSignal(this);
		AutoSlot* pauto = ork::rtti::autocast(pslot);
		if( pauto )
		{
			//ork::object::Disconnect( this, pauto );
		}
	}
}

bool Signal::AddSlot(Object* object, PoolString name)
{
	for(orkvector<Slot*>::iterator it = mSlots.begin(); it != mSlots.end(); it++)
	{
		Slot* ptrslot = (*it);
		if(ptrslot->GetObject() == object && ptrslot->GetSlotName() == name)
		{
			OrkAssert(false);
			return false;
		}
	}
	mSlots.push_back( new Slot(object, name) );
	return true;
}
//////////////////////////////////////////////////////////////////////
bool Signal::AddSlot(Slot* ptrslot)
{
	Object* pothobj = ptrslot->GetObject();
	PoolString pothnam = ptrslot->GetSlotName();

	for(orkvector<Slot*>::iterator it = mSlots.begin(); it != mSlots.end(); it++)
	{
		Slot* conslot = (*it);
		if(conslot->GetObject() == pothobj && conslot->GetSlotName() == pothnam)
		{
			OrkAssert(false);
			return false;
		}
	}
	mSlots.push_back(ptrslot);
	return true;
}
//////////////////////////////////////////////////////////////////////
bool Signal::RemoveSlot(Object* object, PoolString name)
{
	for(orkvector<Slot*>::iterator it = mSlots.begin(); it != mSlots.end(); it++)
	{
		Slot* conslot = (*it);
		if(conslot->GetObject() == object && conslot->GetSlotName() == name)
		{
			mSlots.erase(it);
			return true;
		}
	}
	return false;
}
bool Signal::RemoveSlot(Slot* pslot)
{
	for(orkvector<Slot*>::iterator it = mSlots.begin(); it != mSlots.end(); it++)
	{
		Slot* conslot = (*it);
		if(conslot == pslot)
		{
			mSlots.erase(it);
			return true;
		}
	}
	return false;
}

void Signal::Invoke(reflect::IInvokation* invokation) const
{
	for(orkvector<Slot*>::const_iterator it = mSlots.begin(); it != mSlots.end(); it++)
	{
		Slot* conslot = (*it);
		conslot->Invoke(invokation);
	}
}

reflect::IInvokation* Signal::CreateInvokation() const
{
	if(mSlots.size() > 0)
	{
		Slot* pslot = *mSlots.begin();
		const reflect::IObjectFunctor* functor = pslot->GetFunctor();
		OrkAssertI(functor, "Slot does not have a functor of that name");
		return functor->CreateInvokation();
	}
	return NULL;
}

Object* Signal::GetSlot(size_t index)
{
	OrkAssert(index < mSlots.size());
	return mSlots[index];
}

size_t Signal::GetSlotCount() const
{
	return mSlots.size();
}

void Signal::ResizeSlots(size_t sz)
{
	mSlots.resize(sz);
}

bool Connect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot)
{
	object::ObjectClass* pclass = rtti::downcast<object::ObjectClass*>(pReceiver->GetClass());
	const reflect::Description& descript = pclass->Description();
	const reflect::IObjectFunctor* functor = descript.FindFunctor(slot);
	if(functor != NULL)
	{
		Signal *pSignal = pSender->FindSignal(signal);
		if(pSignal)
			return pSignal->AddSlot(pReceiver, slot);
	}
	return false;
}
bool Connect(Signal* psig, AutoSlot* pslot)
{
	if(psig && pslot )
	{
		bool bsig = psig->AddSlot(pslot);
		bool bslt = true; pslot->AddSignal(psig);
		return (bsig&bslt);
	}
	return false;
}

bool Disconnect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot)
{
	object::ObjectClass* pclass = rtti::downcast<object::ObjectClass*>(pReceiver->GetClass());
	const reflect::Description& descript = pclass->Description();
	const reflect::IObjectFunctor* functor = descript.FindFunctor(slot);
	if(functor != NULL)
	{
		Signal *pSignal = pSender->FindSignal(signal);
		if(pSignal)
			return pSignal->RemoveSlot(pReceiver, slot);
	}
	return false;
}
bool Disconnect(Signal* psig, AutoSlot* pslot)
{
	psig->RemoveSlot(pslot);
	//pslot->RemoveSignal(psig);
	return true;
}

} }
