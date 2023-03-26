////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <string.h>

#include <ork/kernel/orkalloc.h>
#include <ork/orkstd.h>

namespace ork
{

template<bool P, typename WhenFalse, typename WhenTrue> struct select
{};
	
template<typename WhenFalse, typename WhenTrue>
struct select<false, WhenFalse, WhenTrue> 
{
	typedef WhenFalse Type;
};

template<typename WhenFalse, typename WhenTrue>
struct select<true, WhenFalse, WhenTrue> 
{
	typedef WhenTrue Type;
};

// _vector_fwd_traverse
template<typename Data, bool Const>
struct _vector_fwd_traverse
{
	typedef typename select<Const, Data &, const Data &>::Type reference;
	typedef typename select<Const, Data *, const Data *>::Type pointer;
	typedef typename select< true, Data &, const Data &>::Type const_reference;
	typedef typename select< true, Data *, const Data *>::Type const_pointer;
	
	typedef Data value_type;
	
	typedef unsigned int size_type;
	typedef int difference_type;

	void range_assert(pointer a, const_pointer s, const_pointer f) const
	{
		//OrkAssertNotNull(a);
		//OrkAssertNotNull(s);
		//OrkAssertNotNull(f);
		OrkAssertFwdRange(a, s, f);
	}

	template<typename pointer_type>
	pointer_type inc(pointer_type a, difference_type n = 1) const
	{
		//OrkAssertNotNull(a);
		return a + n;
	}

	template<typename pointer_type>
	pointer_type dec(pointer_type a, difference_type n = 1) const
	{
		//OrkAssertNotNull(a);
		return a - n;
	}

	difference_type diff(const_pointer a, const_pointer b) const
	{
		//OrkAssertNotNull(a);
		//OrkAssertNotNull(b);
		return difference_type(a - b);
	}

	bool less(const_pointer a, const_pointer b) const
	{
		return a < b;
	}
	
	bool greater(const_pointer a, const_pointer b) const
	{
		return a > b;
	}	
};

// _vector_bwd_traverse
template<typename Data, bool Const>
struct _vector_bwd_traverse
{
	typedef typename select<Const, Data &, const Data &>::Type reference;
	typedef typename select<Const, Data *, const Data *>::Type pointer;
	typedef typename select< true, Data &, const Data &>::Type const_reference;
	typedef typename select< true, Data *, const Data *>::Type const_pointer;
	typedef Data value_type;
	typedef unsigned int size_type;
	typedef int difference_type;

	void range_assert(pointer a, const_pointer s, const_pointer f) const
	{
		//OrkAssertNotNull(a);
		//OrkAssertNotNull(s);
		//OrkAssertNotNull(f);
		OrkAssertBwdRange(a, s, f);
	}

	template<typename pointer_type>
	pointer_type inc(pointer_type a, difference_type n = 1) const
	{
		//OrkAssertNotNull(a);
		return a - n;
	}

	template<typename pointer_type>
	pointer_type dec(pointer_type a, difference_type n = 1) const
	{
		//OrkAssertNotNull(a);
		return a + n;
	}

	difference_type diff(const_pointer a, const_pointer b) const
	{
		return b - a;
	}

	bool less(const_pointer a, const_pointer b) const
	{
		return b < a;
	}
	
	bool greater(const_pointer a, const_pointer b) const
	{
		return b > a;
	}	
};

// vector_iterator_base
template<typename Data, typename Traverse, bool Const>
struct _vector_iterator_base
{	
	typedef typename select<Const, Data &, const Data &>::Type reference;
	typedef typename select<Const, Data *, const Data *>::Type pointer;
	typedef typename select< true, Data &, const Data &>::Type const_reference;
	typedef typename select< true, Data *, const Data *>::Type const_pointer;
	typedef Data value_type;
	
	typedef unsigned int size_type;
	typedef int difference_type;
	typedef std::random_access_iterator_tag iterator_category;

	typedef Traverse traverse_type;
	traverse_type traverse;

	reference operator*() const
	{
		traverse.range_assert(_at, _start, _finish);
		return *_at;
	}
	
	pointer operator->() const
	{
		return &this->operator *();
	}

	const _vector_iterator_base & operator++()
	{
		traverse.range_assert(_at, _start, _finish);
		_at = traverse.inc(_at);
		return *this;
	}

	_vector_iterator_base operator++(int)
	{
		traverse.range_assert(_at, _start, _finish);
		_vector_iterator_base it = *this;
		_at = traverse.inc(_at);
		return it;
	}

	_vector_iterator_base operator+(difference_type n) const
	{
		return _vector_iterator_base(traverse.inc(_at, n), _start, _finish);
	}
	
	//_vector_iterator_base& operator+(size_type n) const
	//{
	//	traverse.range_assert(traverse.inc(_at, n - 1), _start, _finish);
	//	return _vector_iterator_base(traverse.inc(_at, n), _start, _finish);
	//}

	const _vector_iterator_base& operator+=(difference_type n)
	{
		_at = traverse.inc(_at, n);
		return *this;
	}

	const _vector_iterator_base& operator--()
	{
		_at = traverse.dec(_at);
		return *this;
	}

	_vector_iterator_base operator--(int)
	{
		_vector_iterator_base it = *this;
		_at = traverse.dec(_at);
		return it;
	}

	_vector_iterator_base operator-(difference_type n) const
	{
		return _vector_iterator_base(traverse.dec(_at, n), _start, _finish);
	}

	const _vector_iterator_base& operator-=(difference_type n)
	{
		_at = traverse.dec(_at, n);
		return *this;
	}

	difference_type operator-(const _vector_iterator_base& it) const
	{
		return traverse.diff(_at, it._at);
	}

	reference operator[](size_type n) const
	{
		traverse.range_assert(_at, _start, _finish);
		return *traverse.inc(_at, n);
	}
	
	bool operator==(const _vector_iterator_base& it) const
	{
		return (_at == it._at);
	}

	bool operator!=(const _vector_iterator_base& it) const
	{
		return (_at != it._at);
	}

	bool operator<(const _vector_iterator_base& it) const
	{
		return traverse.less(_at, it._at);
	}
	
	bool operator>(const _vector_iterator_base& it) const
	{
		return traverse.greater(_at, it._at);
	}	
	
	bool operator>=(const _vector_iterator_base& it) const
	{
		return (_at == it._at) || traverse.greater(_at, it._at);
	}
	
	bool operator<=(const _vector_iterator_base& it) const
	{
		return (_at == it._at) || traverse.less(_at, it._at);
	}		

	_vector_iterator_base() : _at(0), _start(0), _finish(0) {}

	_vector_iterator_base(pointer a, const_pointer s, const_pointer f) : _at(a), _start(s), _finish(f) {}

	template<typename _Traverse, bool _Const>
	_vector_iterator_base(const _vector_iterator_base<Data, _Traverse, _Const>& it) : _at(it._at), _start(it._start), _finish(it._finish) {}
	
	template<typename _Traverse, bool _Const>
	_vector_iterator_base& operator=(const _vector_iterator_base<Data, _Traverse, _Const>& it)
	{
		_start = it._start;
		_finish = it._finish;
		_at = it._at;
		return *this;
	}
	
	pointer _at;
	const_pointer _start;
	const_pointer _finish;
};

// vector
template<typename Data, typename Alloc=ork::allocator<Data> >
class vector
{
public:

    typedef Data& reference;
    typedef const Data& const_reference;
    typedef Data value_type;
    typedef value_type *pointer;
	typedef const value_type *const_pointer;
	typedef unsigned int size_type;
	typedef int difference_type;
	typedef Alloc alloc_type;

	typedef _vector_iterator_base<Data, _vector_fwd_traverse<Data, false>, false > iterator;
	typedef _vector_iterator_base<Data, _vector_fwd_traverse<Data, true>, true >  const_iterator;
	typedef _vector_iterator_base<Data, _vector_bwd_traverse<Data, false>, false > reverse_iterator;
	typedef _vector_iterator_base<Data, _vector_bwd_traverse<Data, true>, true >  const_reverse_iterator;

    alloc_type allocator;

	iterator begin()
	{
		return iterator(m_data, m_data, m_data + (m_size - 1));
	}

	const_iterator begin() const
	{
		return const_iterator(m_data, m_data, m_data + (m_size - 1));
	}

	iterator end()
	{
		return iterator(m_data + m_size, m_data, m_data + (m_size - 1));
	}

	const_iterator end() const
	{
		return const_iterator(m_data + m_size, m_data, m_data + (m_size - 1));
	}

	reverse_iterator rbegin()
	{
		return reverse_iterator(m_data + (m_size - 1), m_data + (m_size - 1), m_data);
	}

	const_reverse_iterator rbegin() const
	{
		return const_reverse_iterator(m_data + (m_size - 1), m_data + (m_size - 1), m_data);
	}

	reverse_iterator rend()
	{
		return reverse_iterator(m_data - 1, m_data + (m_size - 1), m_data);
	}

	const_reverse_iterator rend() const
	{
		return const_reverse_iterator(m_data - 1, m_data + (m_size - 1), m_data);
	}

	void pop_back()
	{	
		OrkAssert(m_size > 0);
		m_size--;
	}

    size_type size() const
	{
		return m_size;
	}

    bool empty() const
	{
		return size() == 0;
	}

    size_type max_size()
	{
		return 0x7FFFFFFF;
	}

	void resize(size_type sz, value_type value = Data())
	{
        reserve(sz);
        for(size_type i = m_size; i < sz; i++)
            allocator.construct(&m_data[i], value);

        m_size = sz;
    }

    size_type capacity() const
	{
		return m_capacity;
	}

	void reserve(size_type sz)
	{
		size_type old_capacity = m_capacity;

		if(m_capacity == 0)
		{
			m_capacity = sz;
		}
		
		while(sz > m_capacity)
		{
			if(m_capacity == 0)
				m_capacity = 16;
			else
				m_capacity *= 2;
		}

		if(m_capacity > old_capacity)
		{
			int idatasize = sizeof(Data);
			pointer newdata = allocator.allocate(m_capacity);

			OrkAssert(newdata);

			if(m_data)
				memcpy(newdata, m_data,  idatasize * old_capacity);

			if(m_data)
				allocator.deallocate(m_data, old_capacity);

			m_data = newdata;
		}
	}

    const_reference operator[](size_type i) const
	{
		return m_data[i];
	}

    reference operator[](size_type i)
	{
		return m_data[i];
	}

	void push_back(const_reference value)
	{
		if(m_size == m_capacity)
			reserve(m_size + 1);

		insert(end(), value);
	}

	reference at(size_type i)
	{
		OrkAssert(i >= 0);
		OrkAssert(i < m_size);
		return this->operator[](i);
	}

	const_reference at(size_type i) const
	{
        OrkAssert(i >= 0);
        OrkAssert(i < m_size);
        return this->operator[](i);
    }

    vector(size_type n = 0, const_reference value = value_type()) : m_size(0), m_capacity(0), m_data(0)
	{
		if(n > 0)
		{
			reserve(n);

			for(size_type i = 0; i < n; i++)
				push_back(value);
		}
	}

	vector(const_iterator first, const_iterator last) : m_size(0), m_capacity(0), m_data(0)
	{
		reserve(last - first);

		for(iterator it = first; it != last; it++)
			push_back(*it);
	}

	vector(const vector& v) : m_size(0), m_capacity(0), m_data(0)
	{
		reserve(v.size());

		for(const_iterator it = v.begin(); it != v.end(); it++)
			push_back(*it);
	}

    ~vector()
	{
        for(int i = 0; i < int(m_size); i++)
			allocator.destroy(&m_data[i]);

        allocator.deallocate(m_data, m_capacity);
        m_data = 0;
        m_capacity = 0;
        m_size = 0;
    }

	const vector<value_type, alloc_type>& operator=(const vector& v)
	{
		clear();

		reserve(v.size());

		for(const_iterator it = v.begin(); it != v.end(); it++)
			push_back(*it);

		return *this;
	}

    void clear()
	{
        for(int i = 0; i < int(m_size); i++)
			allocator.destroy(&m_data[i]);
		m_size = 0;
    }

	reference front()
	{
		return *(begin());
	}

	const_reference front() const
	{
		return *(begin());
	}

	reference back()
	{
		return *(rbegin());
	}

	const_reference back() const
	{
		return *(rbegin());
	}

	iterator insert(iterator pos, const_reference value)
	{
		pos.traverse.range_assert(pos._at, pos._start, pos.traverse.inc(pos._finish));

		// NOTE: storing index because pos might be invalidated by reserve()
		difference_type index = pos - begin();

		int isize = int(size());

		if(isize == int(m_capacity))
			reserve((unsigned int)isize + 1);

		//for(pointer p = &m_data[m_size]; p > &m_data[index]; p--)
		int icopycount = isize-index;

		for( int i=(icopycount-1); i>=0; i-- )
		{
			value_type *psrc = & m_data[index+i];
			value_type *pdst = & m_data[index+i+1];

			allocator.construct( pdst, *psrc );
			allocator.destroy( psrc );
		}

		allocator.construct( & m_data[index], value );

		m_size++;

		return iterator( & m_data[index], m_data, m_data + (m_size - 1));
	}

	void insert(iterator pos, const_iterator first, const_iterator last)
	{
		pos.traverse.range_assert(pos._at, pos._start, pos.traverse.inc(pos._finish));
		OrkAssert(first < last);

		// TODO: shift entire range up in one pass
		for(const_iterator it = first; it != last; it++, pos++)
			insert(pos, *it);
	}

	void insert(iterator pos, size_type n, const_reference value)
	{
		pos.traverse.range_assert(pos._at, pos._start, pos.traverse.inc(pos._finish));

		reserve(m_size + n);

		// TODO: shift entire range up in one pass
		for(size_type i = 0; i < n; i++)
			insert(pos, value);
	}

	void erase(iterator pos)
	{
		pos.traverse.range_assert(pos._at, pos._start, pos.traverse.inc(pos._finish));

		allocator.destroy(pos._at);

		m_size--;

		for(pointer p = pos._at; p < &m_data[m_size]; p = pos.traverse.inc(p))
		{
			allocator.construct(p, *pos.traverse.inc(p));
			allocator.destroy(pos.traverse.inc(p));
		}
	}

	void erase(iterator first, iterator last)
	{
		OrkAssert(first < last);

		for(pointer p = first._at; p != last._at; p = first.traverse.inc(p))
			allocator.destroy(p);

		difference_type d = first.traverse.diff(last._at, first._at);
		m_size -= d;
		for(pointer p = first._at; p != first.traverse.dec(last._at); p = first.traverse.inc(p))
		{
			allocator.construct(p, *first.traverse.inc(p, d));
			allocator.destroy(first.traverse.inc(p, d));
		}
	}

private:

    size_type m_size;
    size_type m_capacity;
    pointer m_data;
};

template<typename Data, typename Alloc>
bool operator==(const vector<Data, Alloc>& v1, const vector<Data, Alloc>& v2)
{
	if(&v1 == &v2)
		return true;

	if(v1.size() != v2.size())
		return false;

	typename vector<Data, Alloc>::const_iterator it1 = v1.begin();
	typename vector<Data, Alloc>::const_iterator it2 = v2.begin();
	for(; it1 != v1.end(); it1++, it2++)
		if(*it1 != *it2)
			return false;

	return true;
}

} // namespace ork


