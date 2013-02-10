/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _FU_PARAMETER_H_
#define _FU_PARAMETER_H_

/**
	@file FUParameter.h
	This file contains the FUParameter interface and its many sub-interfaces.
*/

/** @defgroup FUParameter The generic FCollada parameter interface. */

#ifdef WIN32
#pragma warning(disable:4355) // 'this' : used in base member initializer list.
#endif // WIN32

namespace FUParameterQualifiers
{
	enum Qualifiers
	{
		SIMPLE = 0,
		VECTOR = 0,
		COLOR = 1
	};
};


/**
	An interface to a generic FCollada parameter.
	Encapsulates one generic value.
	
	In PREMIUM FCollada: this value may be animatable,
	a list or a complex object, as described
	by higher-level classes. Useful for user interface,
	undo/redo and such advanced features.

	@ingroup FUParameter
*/
template <class TYPE>
class FUParameterT
{
private:
	TYPE value;

public:
	FUParameterT() {}
	FUParameterT(const TYPE& defaultValue) : value(defaultValue) {}
	virtual ~FUParameterT() {}
	inline operator TYPE&() { return value; }
	inline operator const TYPE&() const { return value; }
	inline TYPE& operator *() { return value; }
	inline const TYPE& operator *() const { return value; }
	inline TYPE* operator->() { return &value; }
	inline const TYPE* operator->() const { return &value; }
	FUParameterT<TYPE>& operator= (const TYPE& copy) { value = copy; return *this; }
};

typedef FUParameterT<bool> FUParameterBoolean; /**< A simple floating-point value parameter. */
typedef FUParameterT<float> FUParameterFloat; /**< A simple floating-point value parameter. */
typedef FUParameterT<FMVector2> FUParameterVector2; /**< A 2D vector parameter. */
typedef FUParameterT<FMVector3> FUParameterVector3; /**< A 3D vector parameter. */
typedef FUParameterT<FMVector3> FUParameterColor3; /**< A 3D color parameter. */
typedef FUParameterT<FMVector4> FUParameterVector4; /**< A 4D vector parameter. */
typedef FUParameterT<FMVector4> FUParameterColor4; /**< A 4D color parameter. */
typedef FUParameterT<FMMatrix44> FUParameterMatrix44; /**< A matrix parameter. */
typedef FUParameterT<int32> FUParameterInt32; /**< An integer value parameter. */
typedef FUParameterT<uint32> FUParameterUInt32; /**< An unsigned integer or enumerated-type value parameter. */
typedef FUParameterT<fm::string> FUParameterString; /**< A UTF8 string parameter. */
typedef FUParameterT<fstring> FUParameterFString; /**< A Unicode string parameter. */

typedef fm::vector<float, true> FUParameterFloatList; /**< A simple floating-point value list parameter. */
typedef fm::vector<FMVector2, true> FUParameterVector2List; /**< A 2D vector list parameter. */
typedef fm::vector<FMVector3, true> FUParameterVector3List; /**< A 3D vector list parameter. */
typedef fm::vector<FMVector3, true> FUParameterColor3List; /**< A 3D vector list parameter. */
typedef fm::vector<FMVector4, true> FUParameterVector4List; /**< A 4D vector list parameter. */
typedef fm::vector<FMVector4, true> FUParameterColor4List; /**< A 4D vector list parameter. */
typedef fm::vector<FMMatrix44, true> FUParameterMatrix44List; /**< A matrix list parameter. */
typedef fm::vector<int32, true> FUParameterInt32List; /**< An integer type list parameter. */
typedef fm::vector<uint32, true> FUParameterUInt32List; /**< An unsigned integer or enumerated-type list parameter. */
typedef fm::vector<fm::string, false> FUParameterStringList; /**< A UTF8 string list parameter. */
typedef fm::vector<fstring, false> FUParameterFStringList; /**< A Unicode string list parameter. */

#define DeclareParameter(type, qual, parameterName, niceName) \
	class Parameter_##parameterName : public FUParameterT<type> { \
	public: Parameter_##parameterName() : FUParameterT<type>() {} \
	Parameter_##parameterName(const type& defaultValue) : FUParameterT<type>(defaultValue) {} \
	virtual ~Parameter_##parameterName() {} \
	inline Parameter_##parameterName& operator= (const type& copy) { FUParameterT<type>::operator=(copy); return *this; } \
	} parameterName;

#define DeclareParameterPtr(type, parameterName, niceName) FUTrackedPtr<type> parameterName;
#define DeclareParameterRef(type, parameterName, niceName) FUObjectRef<type> parameterName;
#define DeclareParameterList(list_type, parameterName, niceName) FUParameter##list_type##List parameterName;
#define DeclareParameterTrackList(type, parameterName, niceName) FUTrackedList<type> parameterName;
#define DeclareParameterContainer(type, parameterName, niceName) FUObjectContainer<type> parameterName;

#define ImplementParameterObject(objectClassName, parameterClassName, parameterName, ...)
#define ImplementParameterObjectNoArg(objectClassName, parameterClassName, parameterName)
#define ImplementParameterObjectNoCtr(objectClassName, parameterClassName, parameterName)
#define ImplementParameterObjectT(objectClassName, parameterClassName, parameterName, ...)
#define ImplementParameterObjectNoArgT(objectClassName, parameterClassName, parameterName)
#define ImplementParameterObjectNoCtrT(objectClassName, parameterClassName, parameterName)

#define InitializeParameterNoArg(parameterName) parameterName()
#define InitializeParameter(parameterName, ...) parameterName(__VA_ARGS__)


#if defined(__APPLE__) || defined(LINUX)
#include "FUtils/FUParameter.hpp"
#endif // __APPLE__

#endif // _FCD_PARAMETER_H

