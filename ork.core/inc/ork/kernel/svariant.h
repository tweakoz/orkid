////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
//	static_variants allow you to stick any RAII compliant object
//	   that will fit into into its space (the size is templatized).
//	svar's are handy for passing around objects in an opaque or abstract,
//	   but typesafe way..
//  Basically what differentiates it from other variants
//     is there are no implicit ducktypish autocast operators.
//  ie. If you put an "int" you had better ask for an "int" back.
//     If you attempt to retrieve typed data that does not match
//     what is currenty contained, you will get an assert.
//  You can safely query if what you think is contained actually is.
//	Upon destruction, or resetting of the variant, 
//	   the contained object's destructor will be called to reclaim
//	   any resources which might have been allocated by that object.
//
//  They are not internally thread safe, fyi..
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string.h>
#include <assert.h>
#include <typeinfo>
#include <type_traits>
#include <new>

#include <ork/kernel/atomic.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <int tsize> class static_variant;

///////////////////////////////////////////////////////////////////////////////
// templatized destruction delegate for static_variants

template <int tsize,typename T> struct static_variant_destroyer_t
{
	typedef T MyType;

	static void destroy( static_variant<tsize>& var );
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize,typename T> struct static_variant_copier_t
{
	typedef T MyType;

	static void copy( static_variant<tsize>& lhs, const static_variant<tsize>& rhs );
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize> class static_variant
{
public:
	typedef void (*destroyer_t)( static_variant<tsize>& var );
	typedef void (*copier_t)( static_variant<tsize>& lhs, const static_variant<tsize>& rhs );
	static const int		ksize = tsize;

	//////////////////////////////////////////////////////////////
	// default constuctor
	//////////////////////////////////////////////////////////////
	static_variant()
		: mtinfo(nullptr)
	{
		mDestroyer=nullptr;
		mCopier=nullptr;
	}
	//////////////////////////////////////////////////////////////
	// copy constuctor
	//////////////////////////////////////////////////////////////
	static_variant( const static_variant& oth )
		: mtinfo(nullptr)
	{
		mDestroyer=nullptr;
		mCopier=nullptr;
       	
		auto c = oth.mCopier.load();
		if( c )
			c( *this, oth );
	}
	//////////////////////////////////////////////////////////////
	static_variant& operator = ( const static_variant& oth )
	{
		auto c = oth.mCopier.load();
		if( c )
			c( *this, oth );

		return *this;
	}
	//////////////////////////////////////////////////////////////
	// typed constructor
	//////////////////////////////////////////////////////////////
	template <typename T> static_variant( const T& value )
	{
		static_assert(sizeof(T)<=ksize, "static_variant size violation");
		memset(mbuffer,0,ksize);
		T* pval = (T*) & mbuffer[0];
		new (pval) T(value); 

		AssignDestroyer<T>();
		AssignCopier<T>();

		mtinfo = & typeid( T );
	}
	//////////////////////////////////////////////////////////////
	// destructor, delegate destuction of the contained object to the destroyer
	//////////////////////////////////////////////////////////////
	~static_variant()
	{
		Destroy();
	}
	//////////////////////////////////////////////////////////////
	// call the destroyer on contained object
	//////////////////////////////////////////////////////////////
	void Destroy() 
	{
		destroyer_t pdestr = mDestroyer.exchange(nullptr);
		if( pdestr ) pdestr( *this );
	}
	//////////////////////////////////////////////////////////////
	//	assign a destroyer
	//////////////////////////////////////////////////////////////
	template <typename T> void AssignDestroyer()
	{
		mDestroyer.store(& static_variant_destroyer_t<tsize,T>::destroy);
	}
	//////////////////////////////////////////////////////////////
	//	assign a copier
	//////////////////////////////////////////////////////////////
	template <typename T> void AssignCopier()
	{
		mCopier.store(& static_variant_copier_t<tsize,T>::copy);
	}
	//////////////////////////////////////////////////////////////
	// return true if the contained object is a T
	//////////////////////////////////////////////////////////////
	template <typename T> bool IsA() const
	{
		static_assert(sizeof(T)<=ksize, "static_variant size violation");
		return (mtinfo!=0) ? (*mtinfo)==typeid(T) : false;
	}
	//////////////////////////////////////////////////////////////
	// assign an object to the variant, assert if it does not fit
	//////////////////////////////////////////////////////////////
	template <typename T> void Set( const T& value )
	{
		static_assert(sizeof(T)<=ksize, "static_variant size violation");
		Destroy();
		T* pval = (T*) & mbuffer[0];
		new (pval) T(value); 
		mtinfo = & typeid( T );
		AssignDestroyer<T>();
		AssignCopier<T>();
	}
	//////////////////////////////////////////////////////////////
	// return the type T object by reference, assert if the types dont match
	//////////////////////////////////////////////////////////////
	template <typename T> T& Get()
	{
		static_assert(sizeof(T)<=ksize, "static_variant size violation");
		assert( typeid(T) == *mtinfo );
		T* pval = (T*) & mbuffer[0];
		return *pval;
	}
	//////////////////////////////////////////////////////////////
	// return the type T object by const reference, assert if the types dont match
	//////////////////////////////////////////////////////////////
	template <typename T> const T& Get() const
	{
		static_assert(sizeof(T)<=ksize, "static_variant size violation");
		assert( typeid(T) == *mtinfo );
		const T* pval = (const T*) & mbuffer[0];
		return *pval;
	}
	//////////////////////////////////////////////////////////////
	// return true if the variant is capable of containing an object of type T
	//////////////////////////////////////////////////////////////
	template <typename T> static bool IsTypeOk()
	{	
		int isize = sizeof(T);
		bool rval = (isize<=ksize);
		return rval;
	}
	//////////////////////////////////////////////////////////////
	// return true if the variant has been set to something
	//////////////////////////////////////////////////////////////
	bool IsSet() const { return (mtinfo!=0); }
	
	//////////////////////////////////////////////////////////////
private:
	char					 mbuffer[ksize];
	ork::atomic<destroyer_t> mDestroyer;
	ork::atomic<copier_t>    mCopier;
	const std::type_info*	 mtinfo;
	//////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize,typename T>
inline void static_variant_destroyer_t<tsize,T>::destroy(static_variant<tsize>& var)
{
	var.template Get<T>().~T();
	// just call T's destructor, as opposed to delete
	//	because the variant owns the memory.
	//  aka 'placement delete'
};

template <int tsize,typename T>
inline void static_variant_copier_t<tsize,T>::copy(static_variant<tsize>& lhs, const static_variant<tsize>& rhs)
{
	const T& src = rhs.template Get<T>();
	lhs.template Set<T>(src);
};

///////////////////////////////////////////////////////////////////////////////

static const int kptrsize = sizeof(void*);

typedef static_variant<4> svar4_t;
typedef static_variant<8> svar8_t;
typedef static_variant<16> svar16_t;
typedef static_variant<32> svar32_t;
typedef static_variant<64> svar64_t;
typedef static_variant<96> svar96_t;
typedef static_variant<128> svar128_t;
typedef static_variant<160> svar160_t;
typedef static_variant<192> svar192_t;
typedef static_variant<256> svar256_t;
typedef static_variant<512> svar512_t;
typedef static_variant<1024> svar1024_t;
typedef static_variant<2048> svar2048_t;
typedef static_variant<4096> svar4096_t;
typedef static_variant<kptrsize> svarp_t;

} // namespace ork
