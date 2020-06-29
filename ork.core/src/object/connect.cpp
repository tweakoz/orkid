////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/connect.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>

#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/object/ObjectClass.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::object::ISlot, "ISlot");
INSTANTIATE_TRANSPARENT_RTTI(ork::object::Slot, "Slot");
INSTANTIATE_TRANSPARENT_RTTI(ork::object::AutoSlot, "AutoSlot");
INSTANTIATE_TRANSPARENT_RTTI(ork::object::LambdaSlot, "LambdaSlot");
INSTANTIATE_TRANSPARENT_RTTI(ork::object::Signal, "Signal");

namespace ork { namespace reflect {
template <>
void Serialize<ork::object::Signal>(
    ork::object::Signal const* in,
    ork::object::Signal* out,
    ::ork::reflect::BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi || in;
  } else {
    bidi || out;
  }
}
}} // namespace ork::reflect

namespace ork { namespace reflect {
template <>
void Serialize<ork::object::AutoSlot>(
    ork::object::AutoSlot const* in,
    ork::object::AutoSlot* out,
    ::ork::reflect::BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi || in;
  } else {
    bidi || out;
  }
}
}} // namespace ork::reflect

namespace ork { namespace object {

//////////////////////////////////////////////////////////////////////

void ISlot::Describe() {
  // reflect::RegisterProperty("Object", &ISlot::mObject);
  // reflect::RegisterProperty("SlotName", &ISlot::mSlotName);
}

ISlot::ISlot(Object* object, PoolString name)
    : mSlotName(name)
    , mObject(object) {
}

Object* ISlot::GetObject() const {
  return mObject;
}

void ISlot::SetObject(Object* pobj) {
  mObject = pobj;
}

const PoolString& ISlot::GetSlotName() const {
  return mSlotName;
}

//////////////////////////////////////////////////////////////////////

void Slot::Describe() {
}
Slot::~Slot() {
}
Slot::Slot(Object* object, PoolString name)
    : ISlot(object, name) {
}

const reflect::IObjectFunctor* Slot::GetFunctor() const {
  OrkAssert(mObject);
  object::ObjectClass* clazz = rtti::autocast(mObject->GetClass());
  auto& desc                 = clazz->Description();
  auto classname             = clazz->Name();
  auto f                     = desc.findFunctor(mSlotName);
  return f;
}

void Slot::Invoke(reflect::IInvokation* invokation) const {
  GetFunctor()->invoke(mObject, invokation);
}

//////////////////////////////////////////////////////////////////////

void AutoSlot::Describe() {
  // reflect::RegisterArrayProperty("Signals", &Signal::GetSlot, &Signal::GetSlotCount, &Signal::ResizeSlots);
}

AutoSlot::~AutoSlot() {
  auto copy_of_sigs = mConnectedSignals.atomicCopy();

  for (auto psig : copy_of_sigs)
    psig->RemoveSlot(this);
}

AutoSlot::AutoSlot(Object* object, const char* name)
    : Slot(object, ork::AddPooledString(name)) {
  OrkAssert(object != 0);
}

void AutoSlot::addSignal(Signal* psig) {
  bool found = false;
  mConnectedSignals.atomicOp([&](sig_set_t& ss) {
    for (auto the_sig : ss)
      found |= (the_sig == psig);
    if (false == found)
      ss.insert(psig);
  });
  OrkAssert(false == found);
}

void AutoSlot::RemoveSignal(Signal* psig) {
  mConnectedSignals.atomicOp([&](sig_set_t& ss) {
    orkset<Signal*>::iterator it = ss.find(psig);
    if (it == ss.end()) {
      OrkAssert(false);
    }
    ss.erase(psig);
  });
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void LambdaSlot::Describe() {
  // reflect::RegisterArrayProperty("Signals", &Signal::GetSlot, &Signal::GetSlotCount, &Signal::ResizeSlots);
}

LambdaSlot::LambdaSlot(Object* owner, const char* pname)
    : ISlot(owner, ork::AddPooledString(pname)) {
}

LambdaSlot::~LambdaSlot() {
  for (orkset<Signal*>::const_iterator it = mConnectedSignals.begin(); it != mConnectedSignals.end(); it++) {
    Signal* psig = (*it);
    psig->RemoveSlot(this);
  }
}

void LambdaSlot::Invoke(reflect::IInvokation* invokation) const {
  OrkAssert(false);
}
const reflect::IObjectFunctor* LambdaSlot::GetFunctor() const {
  OrkAssert(false);
  return nullptr;
}

void LambdaSlot::addSignal(Signal* psig) {
  auto it = mConnectedSignals.find(psig);
  if (it != mConnectedSignals.end()) {
    OrkAssert(false);
  }
  mConnectedSignals.insert(psig);
}
void LambdaSlot::RemoveSignal(Signal* psig) {
  auto it = mConnectedSignals.find(psig);
  if (it == mConnectedSignals.end()) {
    OrkAssert(false);
  }
  mConnectedSignals.erase(psig);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Signal::Describe() {
  // reflect::RegisterArrayProperty("Slots", &Signal::GetSlot, &Signal::GetSlotCount, &Signal::ResizeSlots);
}

Signal::Signal() {
}
Signal::~Signal() {
  auto slots = mSlots.atomicCopy();

  for (ISlot* pslot : slots) {
    pslot->RemoveSignal(this);
    AutoSlot* pauto = ork::rtti::autocast(pslot);
    if (pauto) {
      // ork::object::Disconnect( this, pauto );
    }
  }
}

bool Signal::HasSlot(Object* obj, PoolString nam) const {
  const auto& slots = mSlots.LockForRead();

  for (ISlot* pslot : slots) {
    if (pslot->GetObject() == obj && pslot->GetSlotName() == nam) {
      mSlots.UnLock();
      return true;
    }
  }
  mSlots.UnLock();
  return false;
}

bool Signal::AddSlot(Object* object, PoolString name) {
  OrkAssert(false == HasSlot(object, name));

  auto& slots = mSlots.LockForWrite();
  slots.push_back(new Slot(object, name));
  mSlots.UnLock();

  return true;
}
//////////////////////////////////////////////////////////////////////
bool Signal::AddSlot(ISlot* ptrslot) {
  Object* obj    = ptrslot->GetObject();
  PoolString nam = ptrslot->GetSlotName();
  OrkAssert(false == HasSlot(obj, nam));

  auto& slots = mSlots.LockForWrite();
  slots.push_back(ptrslot);
  mSlots.UnLock();
  return true;
}
//////////////////////////////////////////////////////////////////////
bool Signal::RemoveSlot(Object* object, PoolString name) {
  auto& locked_slots = mSlots.LockForWrite();
  for (auto it = locked_slots.begin(); it != locked_slots.end(); it++) {
    auto conslot = *it;
    if (conslot->GetObject() == object && conslot->GetSlotName() == name) {
      locked_slots.erase(it);
      mSlots.UnLock();
      return true;
    }
  }
  mSlots.UnLock();
  return false;
}
bool Signal::RemoveSlot(ISlot* pslot) {
  auto& locked_slots = mSlots.LockForWrite();
  for (auto it = locked_slots.begin(); it != locked_slots.end(); it++) {
    auto conslot = *it;

    if (conslot == pslot) {
      locked_slots.erase(it);
      mSlots.UnLock();
      return true;
    }
  }
  mSlots.UnLock();
  return false;
}

void Signal::Invoke(reflect::IInvokation* invokation) const {
  auto copy_of_slots = mSlots.atomicCopy();

  for (ISlot* conslot : copy_of_slots) {
    conslot->Invoke(invokation);
  }
}

reflect::IInvokation* Signal::CreateInvokation() const {
  auto copy_of_slots = mSlots.atomicCopy();

  if (copy_of_slots.size() > 0) {
    ISlot* pslot                           = *copy_of_slots.begin();
    const reflect::IObjectFunctor* functor = pslot->GetFunctor();
    OrkAssertI(functor, "Slot does not have a functor of that name");
    return functor->CreateInvokation();
  }
  return NULL;
}

ISlot* Signal::GetSlot(size_t index) {
  auto& locked_slots = mSlots.LockForRead();
  auto num_slots     = locked_slots.size();
  bool in_range      = (index < num_slots);
  auto rval          = in_range ? locked_slots[index] : nullptr;
  mSlots.UnLock();
  OrkAssert(in_range);
  return rval;
}

size_t Signal::GetSlotCount() const {
  auto& locked_slots = mSlots.LockForRead();
  auto num_slots     = locked_slots.size();
  mSlots.UnLock();
  return num_slots;
}

void Signal::ResizeSlots(size_t sz) {
  auto& locked_slots = mSlots.LockForWrite();
  locked_slots.resize(sz);
  mSlots.UnLock();
}

//////////////////////////////////////////////////////////////////////
// connect methods
//////////////////////////////////////////////////////////////////////

bool Connect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot) {
  object::ObjectClass* pclass            = rtti::downcast<object::ObjectClass*>(pReceiver->GetClass());
  const reflect::Description& descript   = pclass->Description();
  const reflect::IObjectFunctor* functor = descript.findFunctor(slot);
  if (functor != NULL) {
    Signal* pSignal = pSender->findSignal(signal);
    if (pSignal)
      return pSignal->AddSlot(pReceiver, slot);
  }
  return false;
}
bool Connect(Signal* psig, AutoSlot* pslot) {
  if (psig && pslot) {
    bool bsig = psig->AddSlot(pslot);
    bool bslt = true;
    static_cast<Slot*>(pslot)->addSignal(psig);
    return (bsig & bslt);
  }
  return false;
}

bool ConnectToLambda(Object* pSender, PoolString signal, Object* pReceiver, const slot_lambda_t& slt) {
  Signal* pSignal = pSender->findSignal(signal);
  if (pSignal) {
    assert(false);
    // return pSignal->AddSlot(pReceiver, slot);
  }

  return false;
}

bool Disconnect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot) {
  object::ObjectClass* pclass            = rtti::downcast<object::ObjectClass*>(pReceiver->GetClass());
  const reflect::Description& descript   = pclass->Description();
  const reflect::IObjectFunctor* functor = descript.findFunctor(slot);
  if (functor != NULL) {
    Signal* pSignal = pSender->findSignal(signal);
    if (pSignal)
      return pSignal->RemoveSlot(pReceiver, slot);
  }
  return false;
}
bool Disconnect(Signal* psig, AutoSlot* pslot) {
  psig->RemoveSlot(pslot);
  // pslot->RemoveSignal(psig);
  return true;
}

}} // namespace ork::object
