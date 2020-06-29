////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/reflect/properties/register.h>
#include <ork/kernel/mutex.h>

namespace ork {

PoolString AddPooledLiteral(const ConstString& cs);

namespace reflect {
class IObjectFunctor;
}
} // namespace ork

class Signal;

namespace ork { namespace object {

struct ISlot;
using islot_ptr_t = std::shared_ptr<ISlot>;

typedef std::function<void(Object*)> slot_lambda_t;

struct ISlot : public Object {
  RttiDeclareAbstract(ISlot, Object);

public:
  ISlot(Object* object = 0, PoolString name = AddPooledLiteral(""));

  Object* GetObject() const;
  void SetObject(Object* pobj);

  const PoolString& GetSlotName() const;

  void SetSlotName(const PoolString& sname) {
    mSlotName = sname;
  }

  virtual void addSignal(Signal* psig) {
  }
  virtual void RemoveSignal(Signal* psig) {
  }

  virtual void Invoke(reflect::IInvokation* invokation) const = 0;
  virtual const reflect::IObjectFunctor* GetFunctor() const   = 0;

  PoolString mSlotName;
  Object* mObject;
};

struct Slot : public ISlot {
  RttiDeclareAbstract(Slot, ISlot);

public:
  Slot(Object* object = 0, PoolString name = AddPooledLiteral(""));
  ~Slot();

  const reflect::IObjectFunctor* GetFunctor() const override;
  void Invoke(reflect::IInvokation* invokation) const override;
};

struct AutoSlot : public Slot {
  typedef orkset<Signal*> sig_set_t;

  RttiDeclareAbstract(AutoSlot, Slot);

public:
  AutoSlot(Object* object, const char* pname);
  ~AutoSlot();

private:
  void RemoveSignal(Signal* psig) override;
  void addSignal(Signal* psig) override;
  LockedResource<sig_set_t> mConnectedSignals;
};

struct LambdaSlot : public ISlot {
  typedef orkset<Signal*> sig_set_t;

  RttiDeclareAbstract(LambdaSlot, ISlot);
  LambdaSlot(Object* owner, const char* pname);
  ~LambdaSlot();

private:
  orkset<Signal*> mConnectedSignals;
  void RemoveSignal(Signal* psig) override;
  void addSignal(Signal* psig) override;

  const reflect::IObjectFunctor* GetFunctor() const override;
  void Invoke(reflect::IInvokation* invokation) const override;
};

#define EXPANDLIST01(MACRO) MACRO(01)
#define EXPANDLIST02(MACRO) EXPANDLIST01(MACRO), MACRO(02)
#define EXPANDLIST03(MACRO) EXPANDLIST02(MACRO), MACRO(03)

#define PARAMETER_DECLARATION(N) P##N
#define TYPENAME_LIST(N) typename P##N

class Signal : public Object {
  RttiDeclareAbstract(Signal, Object);

public:
  bool AddSlot(Object* component, PoolString name);
  bool AddSlot(ISlot* pslot);

  bool RemoveSlot(Object* component, PoolString name);
  bool RemoveSlot(ISlot* pslot);

  bool HasSlot(Object* obj, PoolString nam) const;

  void Invoke(reflect::IInvokation* invokation) const;

  reflect::IInvokation* CreateInvokation() const;

  template <typename ReturnType, typename ClassType> void operator()(ReturnType (ClassType::*)());

#define DECLARE_INVOKATION(N)                                                                                                      \
  template <typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)>                                                 \
  void operator()(ReturnType (ClassType::*)(EXPANDLIST##N(PARAMETER_DECLARATION)), EXPANDLIST##N(PARAMETER_DECLARATION));

  DECLARE_INVOKATION(01)
  DECLARE_INVOKATION(02)

#undef DECLARE_INVOKATION

  Signal();
  ~Signal();

private:
  typedef orkvector<ISlot*> slot_set_t;

  ISlot* GetSlot(size_t index);

  size_t GetSlotCount() const;

  void ResizeSlots(size_t sz);

  LockedResource<slot_set_t> mSlots;
};

bool Connect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot);
bool Connect(Signal* psig, AutoSlot* pslot);
bool ConnectToLambda(Object* pSender, PoolString signal, Object* pReceiver, const slot_lambda_t& slt);

bool Disconnect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot);
bool Disconnect(Signal* psig, AutoSlot* pslot);

template <typename ReturnType, typename ClassType> void Signal::operator()(ReturnType (ClassType::*function_signature)()) {
  using FunctionType   = reflect::Function<void (*)()>;
  using InvokationType = reflect::Invokation<typename FunctionType::Parameters>;
  InvokationType invokation;
  Invoke(&invokation);
}

#define PARAMETER_LIST(N) P##N p##N
#define PARAMETER_PASS(N) p##N

#define DEFINE_INVOKATION(N)                                                                                                       \
  template <typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)>                                                 \
  inline void Signal::operator()(                                                                                                  \
      ReturnType (ClassType::*function_signature)(EXPANDLIST##N(PARAMETER_DECLARATION)), EXPANDLIST##N(PARAMETER_LIST)) {          \
    typedef reflect::Function<void (*)(EXPANDLIST##N(PARAMETER_DECLARATION))> FunctionType;                                        \
    typedef reflect::Invokation<typename FunctionType::Parameters> InvokationType;                                                 \
    InvokationType invokation;                                                                                                     \
    reflect::SetParameters(function_signature, invokation.ParameterData(), EXPANDLIST##N(PARAMETER_PASS));                         \
    Invoke(&invokation);                                                                                                           \
  }

DEFINE_INVOKATION(01)
DEFINE_INVOKATION(02)

#undef PARAMETER_LIST
#undef PARAMETER_PASS
#undef TYPENAME_LIST
#undef PARAMETER_DECLARATION

#undef EXPANDLIST01
#undef EXPANDLIST02

///////////////////////////////////////////////////////////////////////////////

#define DeclarePublicSignal(SigName)                                                                                               \
private:                                                                                                                           \
  ork::object::Signal mSignal##SigName;                                                                                            \
                                                                                                                                   \
public:                                                                                                                            \
  ork::object::Signal& GetSig##SigName() {                                                                                         \
    return mSignal##SigName;                                                                                                       \
  }

///////////////////////////////////////////////////////////////////////////////

#define DeclarePublicAutoSlot(SlotName)                                                                                            \
private:                                                                                                                           \
  ork::object::AutoSlot mSlot##SlotName;                                                                                           \
                                                                                                                                   \
public:                                                                                                                            \
  ork::object::AutoSlot& GetSlot##SlotName() {                                                                                     \
    return mSlot##SlotName;                                                                                                        \
  }

///////////////////////////////////////////////////////////////////////////////

#define ConstructAutoSlot(SlotName) mSlot##SlotName(this, "Slot" #SlotName)

///////////////////////////////////////////////////////////////////////////////

#define RegisterAutoSlot(ClassName, SlotName)                                                                                      \
  ork::reflect::RegisterSlot("Slot" #SlotName, &ClassName::mSlot##SlotName, &ClassName::Slot##SlotName);

///////////////////////////////////////////////////////////////////////////////

#define RegisterAutoSignal(ClassName, SignalName) ork::reflect::RegisterSignal("Sig" #SignalName, &ClassName::mSignal##SignalName);

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::object
