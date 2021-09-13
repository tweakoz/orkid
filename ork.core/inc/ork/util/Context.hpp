////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#ifndef _ORK_UTIL_CONTEXT_HPP_
#define _ORK_UTIL_CONTEXT_HPP_

#include <ork/util/Context.h>

#if defined(ORK_CONFIG_DARWIN)
#define USE_MACH_TLS_HACK
#include <ork/kernel/mutex.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/thread_info.h>
#include <dispatch/dispatch.h>
#include <ork/kernel/any.h>
#endif

namespace ork { namespace util {

template <typename T> orkstack<T> GlobalStack<T>::gStack;


////////////////////////////////////////////////////////////////////////////

template <typename T> T& GlobalStack<T>::Top() { return gStack.top(); }
template <typename T> void GlobalStack<T>::Push( T v ) { gStack.push(v); }
template <typename T> void GlobalStack<T>::Pop() { gStack.pop(); }

////////////////////////////////////////////////////////////////////////////

template<typename T>
T *Context<T>::context()
{
	if(NULL != sCurrentContext)
	{
		return sCurrentContext;
	}
	else
	{
		return NULL;
	}
}

///////////////////////////////////////////////////////////

template<typename T>
Context<T>::Context()
	: mPreviousContext(sCurrentContext)
{
	sCurrentContext = static_cast<T *>(this);
}

///////////////////////////////////////////////////////////

template<typename T>
Context<T>::~Context()
{
	OrkAssert(this == sCurrentContext);

	sCurrentContext = mPreviousContext;
}

///////////////////////////////////////////////////////////

template<typename T>
T *Context<T>::sCurrentContext = NULL;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#if defined(USE_MACH_TLS_HACK)

typedef std::map<const std::type_info*,svar64_t> tmap_t;

static tmap_t* get_thr_tmap()
{
	static pthread_key_t key;
	static pthread_once_t key_once = PTHREAD_ONCE_INIT;

	struct yo {
		static void make_key()
		{
	    	(void) pthread_key_create(&key, NULL);
		}
	};

    pthread_once(&key_once, yo::make_key);
    tmap_t* ptmap = (tmap_t*) pthread_getspecific(key);
    if( ptmap == nullptr )
    {	ptmap = new tmap_t;
        pthread_setspecific(key, (void*)ptmap);
    }
    return ptmap;
}
#endif

///////////////////////////////////////////////////////////

template<typename T>
T* ContextTLS<T>::context()
{
	////////////////////
	#if defined(USE_MACH_TLS_HACK)
	////////////////////
	T* rval = nullptr;
    tmap_t* ptmap = get_thr_tmap();
    auto it = ptmap->find(&typeid(T));
    if(it==ptmap->end()) return nullptr;

    svar64_t& sv = it->second;
    if( sv.isA<std::stack<T*>>() )
    {
    	auto& stk = sv.get<std::stack<T*>>();
    	rval = stk.empty() ? nullptr : stk.top();
    }
    return rval;
	////////////////////
	#else
	////////////////////
	if(nullptr != sCurrentContext)
	{
		return sCurrentContext;
	}
	else
	{
		return nullptr;
	}
	////////////////////
	#endif
	////////////////////
}

///////////////////////////////////////////////////////////

template<typename T>
ContextTLS<T>::ContextTLS()
	: mPreviousContext(nullptr)
{
	////////////////////
	#if defined(USE_MACH_TLS_HACK)
	////////////////////
    tmap_t* ptmap = get_thr_tmap();
    auto it = ptmap->find(&typeid(T));
    if(it==ptmap->end())
    {
    	std::stack<T*> def;
    	ptmap->operator[](&typeid(T)).set<std::stack<T*>>(def);
	    it = ptmap->find(&typeid(T));
    }
    assert(it!=ptmap->end());
    std::stack<T*>& stk = it->second.get<std::stack<T*>>();
    stk.push(static_cast<T*>(this));
	////////////////////
	#else
	////////////////////
	mPreviousContext = sCurrentContext;
	sCurrentContext = static_cast<T *>(this);
	////////////////////
	#endif
	////////////////////
}

///////////////////////////////////////////////////////////

template<typename T>
ContextTLS<T>::~ContextTLS()
{
	////////////////////
	#if defined(USE_MACH_TLS_HACK)
	////////////////////
    tmap_t* ptmap = get_thr_tmap();
    auto it = ptmap->find(&typeid(T));
    assert(it!=ptmap->end());
    std::stack<T*>& stk = it->second.get<std::stack<T*>>();
	stk.pop();
	////////////////////
	#else
	////////////////////
	OrkAssert(this == sCurrentContext);
	sCurrentContext = mPreviousContext;
	////////////////////
	#endif
	////////////////////
}

///////////////////////////////////////////////////////////

template<typename T>
ThreadLocal T *ContextTLS<T>::sCurrentContext = NULL;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

} }

#endif
