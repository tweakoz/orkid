////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#if defined( ORK_CONFIG_QT )

#include <ork/kernel/string/string.h>

namespace ork {
std::string MethodIdNameStrip( const char * name );
namespace lev2 {
/*
template< typename T, typename S > 
struct MocMethodVoid : public MocFunctorBase
{
	S mMethod;

	MocMethodVoid( const char *pname, S meth )
		: MocFunctorBase( pname, MethodIdNameStrip( typeid( S ).name() ) )
		, mMethod( meth )
	{

	}

	virtual void Invoke( void *pthis, int _id, void **_a )
	{
		//orkprintf( "MocMethodVoid (pthis %08x) (id %d)\n", pthis, _id );
		T * tstar = static_cast<T*>(pthis);
		(tstar->*mMethod)();
	}

	virtual const char *Params( void ) { return ""; }

};

///////////////////////////////////////////////////////////////////////////////

template< typename T, typename S, typename R >
struct MocMethod1 : public MocFunctorBase
{
	S mMethod;

	MocMethod1( const char *pname, S meth )
		: MocFunctorBase( pname, MethodIdNameStrip( typeid( S ).name() ) )
		, mMethod( meth )
	{

	}

	virtual void Invoke( void *pthis, int _id, void **_a )
	{
		T * tstar = static_cast<T*>(pthis);
		const R * pR = reinterpret_cast<const R*>(_a[1]);
        const R & rR = pR[0];
		(tstar->*mMethod)(rR);
	}

	virtual const char *Params( void ) { return typeid(R).name(); }
};

///////////////////////////////////////////////////////////////////////////////

template< typename T, typename S, typename R0, typename R1 >
struct MocMethod2 : public MocFunctorBase
{
	S mMethod;

	MocMethod2( const char *pname, const char* psig, S meth )
		: MocFunctorBase( pname, MethodIdNameStrip( typeid( S ).name() )  )
		, mMethod( meth )
	{
		mMethodType = ork::CreateFormattedString("%s,%s", typeid(R0).name(),typeid(R1).name());
		//mMethodName = CreateFormattedString("%s(%s)", pname,mMethodType.c_str());
	}

	virtual void Invoke( void *pthis, int _id, void **_a )
	{
		T * tstar = static_cast<T*>(pthis);
		void* a0 = _a[1];
		void* a1 = _a[2];

		const R0 * pR0 = reinterpret_cast<const R0*>(a0);
        const R0 & rR0 = *pR0;
		const R1 * pR1 = reinterpret_cast<const R1*>(a1);
        const R1 & rR1 = *pR1;
		(tstar->*mMethod)(rR0,rR1);
	}

	virtual const char *Params( void )
	{	
		return mMethodType.c_str();
	}
};

///////////////////////////////////////////////////////////////////////////////

template< typename T, typename S, typename R > struct MocMethodSingleRef : public MocFunctorBase
{
	S mMethod;

	MocMethodSingleRef( const char *pname, S meth )
		: MocFunctorBase( pname, MethodIdNameStrip( typeid( S ).name() ) )
		, mMethod( meth )
	{

	}

	virtual void Invoke( void *pthis, int _id, void **_a )
	{
		T * tstar = static_cast<T*>(pthis);
		const R * pR = reinterpret_cast<const R*>(&_a[1]);
        const R & rR = pR[0];
		(tstar->*mMethod)(rR);
	}

	virtual const char *Params( void ) { return typeid(R).name(); }
};

///////////////////////////////////////////////////////////////////////////////

template<typename T, typename P> template<typename S> void CQNoMoc<T,P>::AddSlot0( const char *pname, S method )
{
	MocFunctorBase *methrec = new MocMethodVoid<T,S>( pname, method );
	mSlots.push_back( methrec );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, typename P> template<typename S> void CQNoMoc<T,P>::addSignal0( const char *pname, S method )
{
	MocFunctorBase *methrec = new MocMethodVoid<T,S>( pname, method );
	mSignals.push_back( methrec );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, typename P> template<typename R> void CQNoMoc<T,P>::AddSlot1( const char *pname, void(T::*method)(R) )
{
	MocFunctorBase *methrec = new MocMethod1<T,void(T::*)(R),R>( pname, method );
	mSlots.push_back( methrec );
}

template<typename T, typename P> template<typename R> void CQNoMoc<T,P>::addSignal1( const char *pname, void(T::*method)(R) )
{
	MocFunctorBase *methrec = new MocMethod1<T,void(T::*)(R),R>( pname, method );
	mSignals.push_back( methrec );
}

///////////////////////////////////////////////////////////////////////////////

template<typename T, typename P>
template<typename R0, typename R1> void CQNoMoc<T,P>::AddSlot2( const char *pname, const char* psig, void(T::*method)(R0,R1) )
{
	MocFunctorBase *methrec = new MocMethod2<T,void(T::*)(R0,R1),R0,R1>( pname, psig, method );
	mSlots.push_back( methrec );
}

///////////////////////////////////////////////////////////////////////////////

template <typename Subclass, typename Baseclass>
void MocImp<Subclass,Baseclass>::Emit( Subclass*pobj, const char *pname )
{
	int iqobjsigidx = pobj->metaObject()->indexOfSignal(pname); // relative to QObject
	int ithisoffset = mMoc.GetThisMeta()->methodOffset();
	int iparoffset = mMoc.GetParentMeta()->methodOffset();
	int ithiscount = mMoc.GetThisMeta()->methodCount();
	int iparcount = mMoc.GetParentMeta()->methodCount();
	int ilocsigidx = iqobjsigidx-ithisoffset;

	void *_a[] = { 0 };
	QMetaObject::activate(pobj, mMoc.GetThisMeta(), ilocsigidx, _a);	
}

///////////////////////////////////////////////////////////////////////////////

template <typename Subclass, typename Baseclass> template<typename R>
void MocImp<Subclass,Baseclass>::Emit( Subclass*pobj, const char *pname, const R& val )
{
	int iqobjsigidx = pobj->metaObject()->indexOfSignal(pname); // relative to QObject
	int ithisoffset = mMoc.GetThisMeta()->methodOffset();
	int iparoffset = mMoc.GetParentMeta()->methodOffset();
	int ithiscount = mMoc.GetThisMeta()->methodCount();
	int iparcount = mMoc.GetParentMeta()->methodCount();
	int ilocsigidx = iqobjsigidx-ithisoffset;
	//isigidx -= (ithisoffset-iparoffset); // NOT -ithisoffset
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&val)) };
	QMetaObject::activate(pobj, mMoc.GetThisMeta(), ilocsigidx, _a);	
}

///////////////////////////////////////////////////////////////////////////////

template <typename Subclass, typename Baseclass> 
int MocImp<Subclass,Baseclass>::GetSignalIndex( const char *pname )
{
	int isigidx =  mMoc.GetThisMeta()->indexOfSignal(pname); 
	return isigidx;
}

template <typename Subclass, typename Baseclass>
int MocImp<Subclass,Baseclass>::GetSlotIndex( const char *pname )
{
	int isigidx = mMoc.GetThisMeta()->indexOfSlot(pname); 
	return isigidx;
}*/

///////////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork

///////////////////////////////////////////////////////////////////////////////

#endif
