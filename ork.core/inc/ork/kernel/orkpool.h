////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/Array.h>
///////////////////////////////////////////////////////////////////////////////

namespace ork{

///////////////////////////////////////////////////////////////////////////////

template<typename Data>
class pool
{
public:

	typedef Data& reference;
    typedef const Data& const_reference;
    typedef Data value_type;
    typedef Data* pointer;
	typedef const Data* const_pointer;
	typedef unsigned int size_type;

	pool(size_type count=1) : m_data(count), m_free(count), m_used(count)
	{
		pointer beg = &(*m_data.begin());
		pointer end = &(*m_data.rbegin());
		//printf( "newpool<%p> beg<%p> end<%p> siz<%d>\n", (void*) this, (void*) beg,  (void*) end, count );
		OrkAssert(count > 0);
		clear();
	}
	void reset_size(size_type count)
	{
		OrkAssert(count > 0);
		m_data.resize(count);
		m_free.resize(count);
		m_used.resize(count);
		clear();
	}
	pointer allocate()
	{
		pointer p = 0;
		if(m_free.size() > 0)
		{
			p = m_free[m_free.size() - 1];
			m_free.pop_back();
			m_used.push_back(p);
		}
		return p;
	}

	void deallocate(pointer p)
	{
		OrkAssert(owned(p));
		m_free.push_back(p);
		typename orkvector<pointer>::iterator it = std::find( m_used.begin(), m_used.end(), p );
		OrkAssert(it!=m_used.end());
		m_used.erase(it);
	}

	void construct(pointer p, const_reference value = value_type())
	{
		new(p) value_type(value);
	}

	void destroy(pointer p)
	{
		p->~value_type();
	}

	size_type count()
	{
		return m_free.size();
	}

	size_type size()
	{
		return m_data.size();
	}
	
	bool owned(pointer p)
	{
		pointer beg = &(*m_data.begin());
		pointer end = &(*m_data.rbegin());
		size_t idx = size_t(p) - size_t(beg);
		size_t off = idx % sizeof(value_type);
		//printf( "owned pool<%p> p<%p> beg<%p> end<%p> idx<%d> off<%d>\n", this, p, beg, end, int(idx), int(off) );
		
		return (p >= beg) && (p <= end) && (off==0);
	}

	bool empty()
	{
		return count() == 0;
	}

	void clear()
	{
		m_used.clear();
		m_free.clear();
		for( typename orkvector<value_type>::iterator it = m_data.begin(); it != m_data.end(); it++)
			m_free.push_back(&*it);
	}

	const orkvector<pointer>& used() const { return m_used; }
	
	value_type& direct_access( int idx ) { return m_data[idx]; }
	const value_type& direct_access( int idx ) const { return m_data[idx]; }

protected:

	orkvector<value_type> m_data;
	orkvector<pointer>    m_free;
	orkvector<pointer>    m_used;

};

///////////////////////////////////////////////////////////////////////////////
/**
 * STL allocator that uses a pool to allocate objects. This is NOT a STL-compliant allocator
 * for 3 reasons.
 *
 * 1) It is instantiable. The size of the pool must be specified at construction.
 * 2) It can only be used by STL containers that only ever allocate a single object at a time.
 *    For efficiency purposes, this allocator makes no attempt at detecting contiguous data
 *    items.
 * 3) It can only be used by STL containers that allow an allocator reference to be passed
 *    to the container's constructors.
 *
 * This allocator is useful when the maximum number of data items required is known AND these
 * data items will be allocated and deallocated very frequently.
 *
 * For example, a particle engine may have a list of active particles where this list grows
 * and shrinks every frame. Also, the maximum number of active particles is typically known
 * for a given system.
 *
 *    ork::allocator_pooled<CParticle> gParticleAllocatorPooled(256);
 *    ork::allocator_pooled<list_node<CParticle*> > gParticleNodeAllocatorPooled(256);
 *
 *    ork::list<CParticle*, ork::allocator_pooled<CParticle>,
 *        ork::allocator_pooled<list_node<CParticle*> > >
 *      gParticleList(gParticleAllocatorPooled, gParticleNodeAllocatorPooled);
 *
 * Also, these allocators can be shared, as in the following example:
 *
 *    ork::list<CParticle*, ork::allocator_pooled<CParticle>,
 *        ork::allocator_pooled<list_node<CParticle*> > >
 *      gOpaqueParticleList(gParticleAllocatorPooled, gParticleNodeAllocatorPooled),
 *      gTranspParticleList(gParticleAllocatorPooled, gParticleNodeAllocatorPooled);
 *
 * TODO:
 * - Provide Alloc& and NodeAlloc& arguments to all ork::list constructors so that
 *   the example actually works.
 */
template<typename T>
class allocator_pooled
{
public:

	//typedef Data& reference;
    //typedef const Data& const_reference;
    //typedef Data value_type;
    //typedef Data* pointer;
	//typedef const pointer const_pointer;
	//typedef std::size_t size_type;
	//typedef std::ptrdiff_t difference_type;

	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;

	template<class OtherData>
	struct rebind
	{
		typedef allocator_pooled<OtherData> other;
	};

	pointer address(reference value) const
	{
		return &value;
	}

	const_pointer address(const_reference value) const
	{
		return &value;
	}

	allocator_pooled() /*throw()*/ {}

	allocator_pooled(const allocator_pooled&) /*throw()*/ {}

	template<class U>
	allocator_pooled(const allocator_pooled<U>&) /*throw()*/ {}

	allocator_pooled(size_t size) : m_pool(size) {}

	size_type max_size() const
	{
		return 0x7FFFFFFFUL / sizeof(value_type);
	}

	pointer allocate(size_type num, const void* hint = 0)
	{
		OrkAssert(num == 1);
		return m_pool.allocate();
	}

	void deallocate(pointer p, size_type num)
	{
		OrkAssert(num == 1);
		return m_pool.deallocate(p);
	}

	void construct(pointer p, const_reference value)
	{
		m_pool.construct(p, value);
	}

	void destroy(pointer p)
	{
		m_pool.destroy(p);
	}

protected:

	pool<value_type> m_pool;

};

///////////////////////////////////////////////////////////////////////////////

template<typename Data, size_t ksize>
class fixed_pool
{
public:

	typedef Data& reference;
    typedef const Data& const_reference;
    typedef Data value_type;
    typedef Data* pointer;
	typedef const Data* const_pointer;
	typedef size_t size_type;

	typedef fixedvector<value_type,ksize> datavect_type;
	typedef fixedvector<pointer,ksize> pointervect_type;


	fixed_pool()
	{
		for( size_t i=0; i<ksize; i++ )
		{
			m_data.push_back(Data());
		}
		clear();
		OrkAssert(count() > 0);
	}

	pointer allocate()
	{
		pointer p = 0;
		if(m_free.size() > 0)
		{
			p = m_free[m_free.size() - 1];
			m_free.pop_back();
			m_used.push_back(p);
			new( p ) Data();
		}
		return p;
	}

	void deallocate(pointer p)
	{
		OrkAssert(owned(p));
		m_free.push_back(p);
		typename fixedvector<pointer,ksize>::iterator it = std::find( m_used.begin(), m_used.end(), p );
		OrkAssert(it!=m_used.end());
		m_used.erase(it);
	}

	void construct(pointer p, const_reference value = value_type())
	{
		new(p) value_type(value);
	}

	void destroy(pointer p)
	{
		p->~value_type();
	}

	size_type count() const
	{
		return m_free.size();
	}

	size_type size() const
	{
		return m_data.size();
	}
	
	bool owned(pointer p)
	{
		return p >= &(*m_data.begin()) && p <= &(*m_data.rbegin())
			&& ((((size_t)p) - ((size_t)&(*m_data.begin()))) % sizeof(value_type)) == 0;
	}

	bool empty() const
	{
		return count() == 0;
	}

	size_t capacity() const { return ksize; }

	void clear()
	{
		m_used.clear();
		m_free.clear();
		m_data.resize(ksize);
		for( int i=0; i<ksize; i++ )
		{
			Data* pdata = & m_data[(ksize-1)-i];
			m_free.push_back( pdata );
		}
	}

	const fixedvector<pointer,ksize>& used() const { return m_used; }
    fixedvector<pointer,ksize>& used() { return m_used; }
	
	value_type& direct_access( size_t idx ) { return m_data[idx]; }
	const value_type& direct_access( size_t idx ) const { return m_data[idx]; }

protected:

	fixedvector<value_type,ksize> m_data;
	fixedvector<pointer,ksize>    m_free;
	fixedvector<pointer,ksize>    m_used;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

