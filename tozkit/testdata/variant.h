///////////////////////////////////////////////////////////////////////////////
//  strict_variant
//	copyright 2009, Michael T. Mayers
//  michael@tweakoz.com
//  http://www.tweakoz.com/portfolio/
//  License: go to town
///////////////////////////////////////////////////////////////////////////////

#pragma once

#if 1 //defined(GCC)
#include <typeinfo>
#include <tr1/type_traits>
#elif defined(_WIN32)
#include <type_traits>
#endif

#include <string>
#include <stdarg.h>


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if defined( DEBUG )
#define MyAssert(x) { assert(x); }
#else
#define MyAssert(x) {}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <int tsize> class variant;

///////////////////////////////////////////////////////////////////////////////
// templatized destruction delegate for variants

template <int tsize,typename T> struct variant_destroyer_t
{
	typedef T MyType;

	static void destroy( variant<tsize>& var );
};
template <int tsize,typename T> struct variant_copier_t
{
	typedef T MyType;

	static void copy( variant<tsize>& lhs, const variant<tsize>& rhs );
};

///////////////////////////////////////////////////////////////////////////////
//
//	strict variants allow you to stick any POD that will fit into
//		into its space (whose size is templatized)
//	they are handy for passing around pods in an opaque or abstract,
//	     but typesafe way..
//  it is called a strict variant, maybe it should be called a 'static' 
//       variant? basically what differentiates it from other variants
//       is there are no explicit autocast to multiple type operators
//	
//	only PODS are allowed, since we are copying constructing an object
//	 into a statically sized buffer. The POD can allocate memory upon
//		copy construction if it wants to, but be sure to set bdstroy to true
//		in the constructor or Set method
//
//	if you need a non POD type to go thru a strict variant, then
//		try a pointer, or wrapped pointer in a POD struct. 
//	
//	upon destruction, or resetting of the variant, 
//	 the contained object's destructor will optionally be called to reclaim
//	 any resources which might have been allocated by that object.
//
///////////////////////////////////////////////////////////////////////////////
template <int tsize> class variant 
{
public:
	typedef void (*destroyer_t)( variant<tsize>& var );
	typedef void (*copier_t)( variant<tsize>& lhs, const variant<tsize>& rhs );
	static const int ksize = tsize;

	//////////////////////////////////////////////////////////////
	// default constuctor
	//////////////////////////////////////////////////////////////
	variant()
		: mtinfo(nullptr)
	{
		memset(mbuffer,0,ksize);
		mDestroyer.fetch_and_store(nullptr);
		mCopier.fetch_and_store(nullptr);
	}
	//////////////////////////////////////////////////////////////
	// copy constuctor
	//////////////////////////////////////////////////////////////
	variant( const variant& oth )
		: mtinfo(nullptr)
	{	
		memset(mbuffer,0,ksize);
		mDestroyer.fetch_and_store(nullptr);
		mCopier.fetch_and_store(nullptr);

		if( oth.mCopier )
			oth.mCopier( *this, oth );
	}
	//////////////////////////////////////////////////////////////
	variant& operator = ( const variant& oth )
	{
		if( oth.mCopier )
			oth.mCopier( *this, oth );

		return *this;
	}
	//////////////////////////////////////////////////////////////
	// typed constructor
	//////////////////////////////////////////////////////////////
	template <typename T> variant( const T& value )
	{
		MyAssert(sizeof(T)<=ksize);
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
	~variant()
	{
		Destroy();
	}
	//////////////////////////////////////////////////////////////
	// call the destroyer on contained object
	//////////////////////////////////////////////////////////////
	void Destroy() 
	{
		destroyer_t pdestr = mDestroyer.fetch_and_store(nullptr);
		if( pdestr ) pdestr( *this );
	}
	//////////////////////////////////////////////////////////////
	//	assign a destroyer
	//////////////////////////////////////////////////////////////
	template <typename T> void AssignDestroyer()
	{
		mDestroyer.fetch_and_store(& variant_destroyer_t<tsize,T>::destroy);
	}
	//////////////////////////////////////////////////////////////
	//	assign a copier
	//////////////////////////////////////////////////////////////
	template <typename T> void AssignCopier()
	{
		mCopier.fetch_and_store(& variant_copier_t<tsize,T>::copy);
	}
	//////////////////////////////////////////////////////////////
	// return true if the contained POD object is a T
	//////////////////////////////////////////////////////////////
	template <typename T> bool IsA() const
	{
		MyAssert(sizeof(T)<=ksize);
		return (mtinfo!=0) ? (*mtinfo)==typeid(T) : false;
	}
	//////////////////////////////////////////////////////////////
	// assign a POD object to the variant, assert if its not a POD
	//////////////////////////////////////////////////////////////
	template <typename T> void Set( const T& value )
	{
		MyAssert(sizeof(T)<=ksize);
		Destroy();
		T* pval = (T*) & mbuffer[0];
		new (pval) T(value); 
		mtinfo = & typeid( T );
		AssignDestroyer<T>();
		AssignCopier<T>();
	}
	//////////////////////////////////////////////////////////////
	// return the POD of type T object by reference, assert if the types dont match
	//////////////////////////////////////////////////////////////
	template <typename T> T& Get()
	{
		MyAssert(sizeof(T)<=ksize);
		MyAssert( typeid(T) == *mtinfo );
		T* pval = (T*) & mbuffer[0];
		return *pval;
	}
	//////////////////////////////////////////////////////////////
	// GCC only likes this version inside of another template, perhaps because it does not override
	//	on return type only?
	//////////////////////////////////////////////////////////////
	template <typename T> void Get(T*& pval)
	{
		MyAssert(sizeof(T)<=ksize);
		MyAssert( typeid(T) == *mtinfo );
		pval = (T*) & mbuffer[0];
	}
	//////////////////////////////////////////////////////////////
	// return the POD of type T object by const reference, assert if the types dont match
	//////////////////////////////////////////////////////////////
	template <typename T> const T& Get() const
	{
		MyAssert(sizeof(T)<=ksize);
		MyAssert( typeid(T) == *mtinfo );
		const T* pval = (const T*) & mbuffer[0];
		return *pval;
	}
	//////////////////////////////////////////////////////////////
	// return true if the variant is capable of containing an object of type T
	//////////////////////////////////////////////////////////////
	template <typename T> static bool IsTypeOk()
	{	
		int isize = sizeof(T);
		bool rval = (isize<=ksize) && std::tr1::is_pod<T>::value;
		return rval;
	}
	//////////////////////////////////////////////////////////////
	// return true if the variant has been set to something
	//////////////////////////////////////////////////////////////
	bool IsSet() const { return (mtinfo!=0); }
	
	//////////////////////////////////////////////////////////////
private:
	char					 mbuffer[ksize];
	tbb::atomic<destroyer_t> mDestroyer;
	tbb::atomic<copier_t>    mCopier;
	const std::type_info*	 mtinfo;
	//////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize,typename T>
inline void variant_destroyer_t<tsize,T>::destroy(variant<tsize>& var)
{
	T* pT = 0;
	var.Get(pT);
	pT->~T();	// just call pT's destructor,
				//	because the variant owns the memory occupied by *pT
};

template <int tsize,typename T>
inline void variant_copier_t<tsize,T>::copy(variant<tsize>& lhs, const variant<tsize>& rhs)
{
	const T& src = rhs.template Get<T>();
	lhs.template Set<T>(src);
	//const T& dst = lhs.template Get<T>();
};

///////////////////////////////////////////////////////////////////////////////

static const int kptrsize = sizeof(void*);

typedef variant<4> variant4;
typedef variant<16> variant16;
typedef variant<64> variant64;
typedef variant<128> variant128;
typedef variant<256> variant256;
typedef variant<512> variant512;
typedef variant<1024> variant1024;
typedef variant<kptrsize> variantp;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


