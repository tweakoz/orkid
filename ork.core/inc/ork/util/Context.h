////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#pragma once

#include <ork/orkconfig.h>
#include <stack>
#include <memory>

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
struct GlobalStack
{
    using stack_t = std::stack<T> ;
    using stack_ptr_t = std::shared_ptr<stack_t>;

    static stack_ptr_t g_stack();
    static T& top();
    static void push( T v );
    static void pop();

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
