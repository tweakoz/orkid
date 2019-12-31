////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#pragma once

#include <ork/orkconfig.h>
#include <stack>

namespace ork { namespace util {

/// Contexts are used for passing data across the stack, rather than through it.
///
/// Any instance of a class which subclasses Context becomes globally accessable
/// through its static context() method. (Think local scope, as opposed to lexical scope)
///
/// Contexts are a template injection class, therefore T should be the class with
/// which you are subclassing Context.
///

// Don't forget to instantiate your stuff too:

template <typename T>
class GlobalStack
{
    public:
    static std::stack<T> gStack;
    static T& Top();
    static void Push( T v );
    static void Pop();

};

template<typename T>
class Context
{
protected:
	Context();
public:
	/// Get the most recently constructed instance of T.
	static T *context();
	~Context();
protected:
	T *PreviousContext() const { return mPreviousContext; }
private:
	T* mPreviousContext;
	static T* sCurrentContext;
};

template<typename T>
class ContextTLS
{
protected:
	ContextTLS();
public:
	/// Get the most recently constructed instance of T.
	static T *context();
	~ContextTLS();
protected:
	T *PreviousContext() const { return mPreviousContext; }
private:
	T* mPreviousContext;
	static ThreadLocal T* sCurrentContext;
};

} } // namespace ork::util
