////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

typedef std::function<void(object_ptr_t)> slot_lambda_t;

/////////////////////////////////////////////////////////////////////////////////////////

struct ISlot : public Object {

  RttiDeclareAbstract(ISlot, Object);

public:
  ISlot(std::string name="");

  object_ptr_t GetObject() const;
  void SetObject(object_ptr_t pobj);
  inline void attach(object_ptr_t object){
    SetObject(object);
  }

  const std::string& GetSlotName() const;

  void SetSlotName(const std::string& sname) {
    mSlotName = sname;
  }

  virtual void addSignal(Signal* psig) {
  }
  virtual void RemoveSignal(Signal* psig) {
  }

  virtual void Invoke(reflect::IInvokation* invokation) const = 0;
  virtual const reflect::IObjectFunctor* GetFunctor() const   = 0;

  std::string mSlotName;
  object_ptr_t _object;
};

/////////////////////////////////////////////////////////////////////////////////////////

struct Slot : public ISlot {
  RttiDeclareAbstract(Slot, ISlot);

public:
  Slot(std::string name = "");
  ~Slot();

  const reflect::IObjectFunctor* GetFunctor() const override;
  void Invoke(reflect::IInvokation* invokation) const override;
};

/////////////////////////////////////////////////////////////////////////////////////////

struct AutoSlot : public Slot {
  typedef orkset<Signal*> sig_set_t;

  RttiDeclareAbstract(AutoSlot, Slot);

public:
  AutoSlot(std::string name = "");
  ~AutoSlot();

private:
  void RemoveSignal(Signal* psig) override;
  void addSignal(Signal* psig) override;
  LockedResource<sig_set_t> mConnectedSignals;
};

using autoslot_ptr_t = std::shared_ptr<AutoSlot>;

/////////////////////////////////////////////////////////////////////////////////////////

struct LambdaSlot : public ISlot {
  typedef orkset<Signal*> sig_set_t;

  RttiDeclareAbstract(LambdaSlot, ISlot);
  LambdaSlot(std::string name = "");
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

#define PARAMETER_LIST(N) P##N p##N
#define PARAMETER_PASS(N) p##N

#define DECLARE_SIGNAL_INVOKATION(N)                                                                                               \
  template <typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)>                                                 \
  void operator()(ReturnType (ClassType::*)(EXPANDLIST##N(PARAMETER_DECLARATION)), EXPANDLIST##N(PARAMETER_DECLARATION));

/////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_SIGNAL_OPERATOR(N)                                                                                                       \
  template <typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)>                                                 \
  void Signal::operator();

#define DEFINE_SIGNAL_OPERATOR(N)                                                                                                       \
  template <typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)>                                                 \
  void Signal::operator()(                                                                                                  \
      ReturnType (ClassType::*function_signature)(EXPANDLIST##N(PARAMETER_DECLARATION)), EXPANDLIST##N(PARAMETER_LIST)) {          \
    typedef reflect::Function<void (*)(EXPANDLIST##N(PARAMETER_DECLARATION))> FunctionType;                                        \
    typedef reflect::Invokation<typename FunctionType::Parameters> InvokationType;                                                 \
    InvokationType invokation;                                                                                                     \
    reflect::SetParameters(function_signature, invokation.ParameterData(), EXPANDLIST##N(PARAMETER_PASS));                         \
    Invoke(&invokation);                                                                                                           \
  }

/////////////////////////////////////////////////////////////////////////////////////////

class Signal : public Object {
  RttiDeclareAbstract(Signal, Object);

public:
  bool AddSlot(object_ptr_t component, std::string name);
  bool AddSlot(ISlot* pslot);

  bool RemoveSlot(object_ptr_t component, std::string name);
  bool RemoveSlot(ISlot* pslot);

  bool HasSlot(object_ptr_t obj, std::string nam) const;

  void Invoke(reflect::IInvokation* invokation) const;

  reflect::IInvokation* CreateInvokation() const;

  template <typename ReturnType, typename ClassType> void operator()(ReturnType (ClassType::*)());

  DECLARE_SIGNAL_INVOKATION(01);
  DECLARE_SIGNAL_INVOKATION(02);

  Signal();
  ~Signal();

private:
  typedef orkvector<ISlot*> slot_set_t;

  ISlot* GetSlot(size_t index);

  size_t GetSlotCount() const;

  void ResizeSlots(size_t sz);

  LockedResource<slot_set_t> mSlots;
};

/////////////////////////////////////////////////////////////////////////////////////////

bool Connect(object_ptr_t pSender, std::string signal, object_ptr_t pReceiver, std::string slot);
bool Connect(Signal* psig, AutoSlot* pslot);
bool ConnectToLambda(object_ptr_t pSender, std::string signal, object_ptr_t pReceiver, const slot_lambda_t& slt);

bool Disconnect(object_ptr_t pSender, std::string signal, object_ptr_t pReceiver, std::string slot);
bool Disconnect(Signal* psig, AutoSlot* pslot);

/////////////////////////////////////////////////////////////////////////////////////////

template <typename ReturnType, typename ClassType> //
void Signal::operator()(ReturnType (ClassType::*function_signature)()) { //
  using FunctionType   = reflect::Function<void (*)()>;
  using InvokationType = reflect::Invokation<typename FunctionType::Parameters>;
  InvokationType invokation;
  Invoke(&invokation);
}

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

#define ConstructAutoSlot(SlotName) mSlot##SlotName("Slot" #SlotName)
#define attachAutoSlot(SlotName) mSlot##SlotName.attach(shared_this);

///////////////////////////////////////////////////////////////////////////////

#define RegisterAutoSlot(ClassName, SlotName)                                                                                      \
  ork::reflect::RegisterSlot("Slot" #SlotName, &ClassName::mSlot##SlotName, &ClassName::Slot##SlotName);

///////////////////////////////////////////////////////////////////////////////

#define RegisterAutoSignal(ClassName, SignalName) ork::reflect::RegisterSignal("Sig" #SignalName, &ClassName::mSignal##SignalName);

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::object
