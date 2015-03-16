////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/kernel/mutex.h>

namespace ork {

PoolString AddPooledLiteral(const ConstString &cs);

namespace reflect {
class IObjectFunctor;
} }

class Object;
class Signal;

namespace ork { namespace object {

typedef std::function<void(Object*)> slot_lambda_t;

struct ISlot : public Object
{
	RttiDeclareAbstract(ISlot,Object);

public:

	ISlot(Object* object = 0, PoolString name = AddPooledLiteral(""));

	Object* GetObject() const;
	void SetObject( Object* pobj );

	const PoolString& GetSlotName() const;

	void SetSlotName( const PoolString& sname ) { mSlotName=sname; }

	virtual void AddSignal( Signal* psig ) {}
	virtual void RemoveSignal( Signal* psig ) {}

	virtual void Invoke(reflect::IInvokation* invokation) const = 0;
	virtual const reflect::IObjectFunctor* GetFunctor() const = 0;

	PoolString mSlotName;
	Object* mObject;
};

struct Slot : public ISlot
{
	RttiDeclareAbstract(Slot,ISlot);

public:

	Slot( Object* object = 0, PoolString name = AddPooledLiteral(""));
	~Slot();

	const reflect::IObjectFunctor* GetFunctor() const override;
	void Invoke(reflect::IInvokation* invokation) const override;


};

struct AutoSlot : public Slot
{
	typedef orkset<Signal*>	sig_set_t;

	RttiDeclareAbstract(AutoSlot,Slot);
public:
	AutoSlot(Object* object, const char* pname );
	~AutoSlot();
private:
	void RemoveSignal( Signal* psig ) override;
	void AddSignal( Signal* psig ) override;
	LockedResource<sig_set_t>	mConnectedSignals;
};

struct LambdaSlot : public ISlot
{
	typedef orkset<Signal*>	sig_set_t;

	RttiDeclareAbstract(LambdaSlot,ISlot);
	LambdaSlot( Object* owner, const char* pname );
	~LambdaSlot();
private:
	orkset<Signal*>	mConnectedSignals;
	void RemoveSignal( Signal* psig ) override; 
	void AddSignal( Signal* psig ) override;

	const reflect::IObjectFunctor* GetFunctor() const override;
	void Invoke(reflect::IInvokation* invokation) const override;
};

#define EXPANDLIST01(MACRO)                      MACRO(01)
#define EXPANDLIST02(MACRO) EXPANDLIST01(MACRO), MACRO(02)
#define EXPANDLIST03(MACRO) EXPANDLIST02(MACRO), MACRO(03)
#define EXPANDLIST04(MACRO) EXPANDLIST03(MACRO), MACRO(04)
#define EXPANDLIST05(MACRO) EXPANDLIST04(MACRO), MACRO(05)
#define EXPANDLIST06(MACRO) EXPANDLIST05(MACRO), MACRO(06)
#define EXPANDLIST07(MACRO) EXPANDLIST06(MACRO), MACRO(07)
#define EXPANDLIST08(MACRO) EXPANDLIST07(MACRO), MACRO(08)
#define EXPANDLIST09(MACRO) EXPANDLIST08(MACRO), MACRO(09)
#define EXPANDLIST10(MACRO) EXPANDLIST09(MACRO), MACRO(10)
#define EXPANDLIST11(MACRO) EXPANDLIST10(MACRO), MACRO(11)
#define EXPANDLIST12(MACRO) EXPANDLIST11(MACRO), MACRO(12)
#define EXPANDLIST13(MACRO) EXPANDLIST12(MACRO), MACRO(13)
#define EXPANDLIST14(MACRO) EXPANDLIST13(MACRO), MACRO(14)
#define EXPANDLIST15(MACRO) EXPANDLIST14(MACRO), MACRO(15)
#define EXPANDLIST16(MACRO) EXPANDLIST15(MACRO), MACRO(16)

#define PARAMETER_DECLARATION(N) P##N
#define TYPENAME_LIST(N) typename P##N

class Signal : public Object
{
	RttiDeclareAbstract(Signal,Object);

public:

	bool AddSlot(Object* component, PoolString name);
	bool AddSlot(ISlot* pslot);

	bool RemoveSlot(Object* component, PoolString name);
	bool RemoveSlot(ISlot* pslot);

	bool HasSlot( Object* obj, PoolString nam ) const;

	void Invoke(reflect::IInvokation* invokation) const;
	
	reflect::IInvokation* CreateInvokation() const;

	template<typename ReturnType, typename ClassType>
	void operator()(ReturnType (ClassType::*)());

	
#define DECLARE_INVOKATION(N) \
	template<typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)> \
	void operator()(ReturnType (ClassType::*)(EXPANDLIST##N(PARAMETER_DECLARATION)), EXPANDLIST##N(PARAMETER_DECLARATION));

	DECLARE_INVOKATION(01)
	DECLARE_INVOKATION(02)
	DECLARE_INVOKATION(03)
	DECLARE_INVOKATION(04)
	DECLARE_INVOKATION(05)
	DECLARE_INVOKATION(06)
	DECLARE_INVOKATION(07)
	DECLARE_INVOKATION(08)
	DECLARE_INVOKATION(09)
	DECLARE_INVOKATION(10)
	DECLARE_INVOKATION(11)
	DECLARE_INVOKATION(12)
	DECLARE_INVOKATION(13)
	DECLARE_INVOKATION(14)
	DECLARE_INVOKATION(15)
	DECLARE_INVOKATION(16)

#undef DECLARE_INVOKATION

	Signal();
	~Signal();

private:

	typedef orkvector<ISlot*>	slot_set_t;

	Object* GetSlot(size_t index);

	size_t GetSlotCount() const;

	void ResizeSlots(size_t sz);

	LockedResource<slot_set_t> mSlots;

};

bool Connect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot);
bool Connect(Signal* psig, AutoSlot* pslot);
bool ConnectToLambda(Object* pSender, PoolString signal, Object* pReceiver, const slot_lambda_t& slt );

bool Disconnect(Object* pSender, PoolString signal, Object* pReceiver, PoolString slot);
bool Disconnect(Signal* psig, AutoSlot* pslot);




template<typename ReturnType, typename ClassType>
void Signal::operator()(ReturnType (ClassType::*function_signature)())
{
	typedef reflect::Function<void (*)()> FunctionType;
	typedef reflect::Invokation<typename FunctionType::Parameters> InvokationType;
	InvokationType invokation;
	Invoke(&invokation);
}

#define PARAMETER_LIST(N) P##N p##N
#define PARAMETER_PASS(N) p##N

#define DEFINE_INVOKATION(N) \
template<typename ReturnType, typename ClassType, EXPANDLIST##N(TYPENAME_LIST)> \
inline void Signal::operator()(ReturnType (ClassType::*function_signature)(EXPANDLIST##N(PARAMETER_DECLARATION)), EXPANDLIST##N(PARAMETER_LIST)) \
{ \
	typedef reflect::Function<void (*)(EXPANDLIST##N(PARAMETER_DECLARATION))> FunctionType; \
	typedef reflect::Invokation<typename FunctionType::Parameters> InvokationType; \
	InvokationType invokation; \
	reflect::SetParameters(function_signature, invokation.ParameterData(), EXPANDLIST##N(PARAMETER_PASS)); \
	Invoke(&invokation); \
}

DEFINE_INVOKATION(01)
DEFINE_INVOKATION(02)
DEFINE_INVOKATION(03)
DEFINE_INVOKATION(04)
DEFINE_INVOKATION(05)
DEFINE_INVOKATION(06)
DEFINE_INVOKATION(07)
DEFINE_INVOKATION(08)
DEFINE_INVOKATION(09)
DEFINE_INVOKATION(10)
DEFINE_INVOKATION(11)
DEFINE_INVOKATION(12)
DEFINE_INVOKATION(13)
DEFINE_INVOKATION(14)
DEFINE_INVOKATION(15)
DEFINE_INVOKATION(16)

#undef PARAMETER_LIST
#undef PARAMETER_PASS
#undef TYPENAME_LIST
#undef PARAMETER_DECLARATION

#undef EXPANDLIST01
#undef EXPANDLIST02
#undef EXPANDLIST03
#undef EXPANDLIST04
#undef EXPANDLIST05
#undef EXPANDLIST06
#undef EXPANDLIST07
#undef EXPANDLIST08
#undef EXPANDLIST09
#undef EXPANDLIST10
#undef EXPANDLIST11
#undef EXPANDLIST12
#undef EXPANDLIST13
#undef EXPANDLIST14
#undef EXPANDLIST15
#undef EXPANDLIST16

///////////////////////////////////////////////////////////////////////////////

#define DeclarePublicSignal( SigName )\
private:\
ork::object::Signal mSignal##SigName;\
public:\
ork::object::Signal& GetSig##SigName() { return mSignal##SigName; }

///////////////////////////////////////////////////////////////////////////////

#define DeclarePublicAutoSlot( SlotName )\
private:\
ork::object::AutoSlot mSlot##SlotName;\
public:\
ork::object::AutoSlot& GetSlot##SlotName() { return mSlot##SlotName; }

///////////////////////////////////////////////////////////////////////////////

#define ConstructAutoSlot( SlotName )\
 mSlot##SlotName( this, "Slot"#SlotName )

///////////////////////////////////////////////////////////////////////////////

#define RegisterAutoSlot( ClassName, SlotName )\
ork::reflect::RegisterSlot( "Slot"#SlotName, &ClassName::mSlot##SlotName, &ClassName::Slot##SlotName );

///////////////////////////////////////////////////////////////////////////////

#define RegisterAutoSignal( ClassName, SignalName )\
ork::reflect::RegisterSignal( "Sig"#SignalName, &ClassName::mSignal##SignalName );

///////////////////////////////////////////////////////////////////////////////

} }
