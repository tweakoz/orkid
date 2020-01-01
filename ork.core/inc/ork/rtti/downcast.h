////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/ICastable.h>
#include <ork/orkstd.h>

namespace ork { namespace rtti {

template<typename T> struct ClassTypeOf { typedef T Type; }; 
template<typename T> struct ClassTypeOf<const T> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<volatile T> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<const volatile T> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<T &> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<const T &> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<volatile T &> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<const volatile T &> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<T *> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<const T *> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<volatile T *> { typedef T Type; }; 
template<typename T> struct ClassTypeOf<const volatile T *> { typedef T Type; }; 

///
/// Produces an "other" type which matches the type T in terms of
/// pointer/reference, const, and volatile qualifiers.
///
/// @param T  The reference or pointer type to convert from.
///
template<typename T, typename Other> struct CompatibleCast { typedef Other Type; }; 
template<typename T, typename Other> struct CompatibleCast<const T, Other> { typedef const Other Type; }; 
template<typename T, typename Other> struct CompatibleCast<volatile T, Other> { typedef volatile Other Type; }; 
template<typename T, typename Other> struct CompatibleCast<const volatile T, Other> { typedef const volatile Other Type; }; 
template<typename T, typename Other> struct CompatibleCast<T &, Other> { typedef Other &Type; }; 
template<typename T, typename Other> struct CompatibleCast<const T &, Other> { typedef const Other &Type; };
template<typename T, typename Other> struct CompatibleCast<volatile T &, Other> { typedef volatile Other &Type; };
template<typename T, typename Other> struct CompatibleCast<const volatile T &, Other> { typedef const volatile Other &Type; };
template<typename T, typename Other> struct CompatibleCast<T *, Other> { typedef Other *Type; }; 
template<typename T, typename Other> struct CompatibleCast<const T *, Other> { typedef const Other *Type; }; 
template<typename T, typename Other> struct CompatibleCast<volatile T *, Other> { typedef volatile Other *Type; }; 
template<typename T, typename Other> struct CompatibleCast<const volatile T *, Other> { typedef const volatile Other *Type; }; 

///
/// Provides a function which undoes the effects of MakePointer, 
/// converting the pointer back to a reference when T is a reference.
///
/// @param T  The reference or pointer type to convert from.
///
template<typename T> struct MakePointer { };
template<typename T> struct MakePointer<T &> { static inline T *Apply(T &x) { return &x; } };
template<typename T> struct MakePointer<T *> { static inline T *Apply(T *x) { return x; } };

///
/// Provides a function which undoes the effects of MakePointer, 
/// converting the pointer back to a reference when T is a reference.
///
/// @param T  The reference or pointer type to convert to.
///
template<typename T> struct UnmakePointer { };
template<typename T> struct UnmakePointer<T &> { static inline T &Apply(T *x) { return *x; } };
template<typename T> struct UnmakePointer<T *> { static inline T *Apply(T *x) { return x; } };

///
/// downcasts the RTTI object to the TargetType if possible.
/// @param object An ICastable.
///
template<typename TargetType>
static inline TargetType downcast(typename CompatibleCast<TargetType, ICastable>::Type object)
{
    typedef typename CompatibleCast<TargetType, ICastable>::Type ObjectType;
    typedef typename ClassTypeOf<TargetType>::Type ClassType;
    if(MakePointer<ObjectType>::Apply(object) == NULL)
		return UnmakePointer<TargetType>::Apply(NULL);

    Class *target = ClassType::GetClassStatic();
	Class *object_class = MakePointer<ObjectType>::Apply(object)->GetClass();

	if(object_class->IsSubclassOf(target))
       return static_cast<TargetType>(object);

    return UnmakePointer<TargetType>::Apply(NULL);
}

template<>
inline ICastable& downcast<ICastable&>(ICastable &object) { return object; }
template<>
inline ICastable *downcast<ICastable *>(ICastable *object) { return object; }
template<>
inline const ICastable &downcast<const ICastable &>(const ICastable &object) { return object; }
template<>
inline const ICastable *downcast<const ICastable *>(const ICastable *object) { return object; }
template<>
inline volatile ICastable &downcast<volatile ICastable &>(volatile ICastable &object) { return object; }
template<>
inline volatile ICastable *downcast<volatile ICastable *>(volatile ICastable *object) { return object; }
template<>
inline const volatile ICastable &downcast<const volatile ICastable &>(const volatile ICastable &object) { return object; }
template<>
inline const volatile ICastable *downcast<const volatile ICastable *>(const volatile ICastable *object) { return object; }


///
/// downcasts the RTTI object to the TargetType if possible.
/// @param object An ICastable.
///
template<typename TargetType>
inline TargetType safe_downcast(typename CompatibleCast<TargetType, ICastable>::Type object)
{
	TargetType result = 0;

	if( object )
	{
		result = downcast<TargetType>(object);
		OrkAssert(MakePointer<TargetType>::Apply(result));
	}

	return result;
}

class AutoCaster
{
public:
    AutoCaster(ICastable *object)
        : mObject(object) 
    {}

    template<typename T> operator T *() const
    {
        return static_cast<T *>(T::GetClassStatic()->Cast(mObject));
    }
private:
    ICastable *const mObject;
};

class AutoCasterConst
{
public:
    AutoCasterConst(const ICastable *object)
        : mObject(object) 
    {}

    template<typename T> operator const T *() const
    {
        return static_cast<const T *>(T::GetClassStatic()->Cast(mObject));
    }
private:
    const ICastable *const mObject;
};

static inline AutoCaster autocast(ICastable *castable) { return AutoCaster(castable); }
static inline AutoCasterConst autocast(const ICastable *castable) { return AutoCasterConst(castable); }

} }
