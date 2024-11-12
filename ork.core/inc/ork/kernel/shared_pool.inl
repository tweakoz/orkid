////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/Array.h>
///////////////////////////////////////////////////////////////////////////////

namespace ork::shared_pool{

template<typename Data, size_t ksize>
class fixed_pool
{
public:

	typedef Data& reference;
    typedef const Data& const_reference;
    typedef Data value_type;
	typedef size_t size_type;

    using pointer_t = std::shared_ptr<Data>;
    using const_pointer_t = std::shared_ptr<const Data>;
	using datavect_t = fixedvector<pointer_t,ksize>;


	fixed_pool() {
		for( size_t i=0; i<ksize; i++ ) {
			auto item = std::make_shared<Data>();
			_data.push_back(item);
			_free.push_back(item);
		}
		OrkAssert(count() > 0);
	}

	pointer_t allocate() {
		pointer_t p = nullptr;
		if(_free.size() > 0) {
			p = _free[_free.size() - 1];
			_free.pop_back();
			_used.push_back(p);
		}
		return p;
	}

	void deallocate(pointer_t p) {
		OrkAssert(owned(p));
		_free.push_back(p);
		auto it = std::find( _used.begin(), _used.end(), p );
		OrkAssert(it!=_used.end());
		_used.erase(it);
	}

	size_type count() const {
		return _free.size();
	}

	size_type size() const {
		return _data.size();
	}
	
	bool owned(pointer_t p) {
		auto it = std::find( _data.begin(), _data.end(), p );
		return it != _data.end();
	}

	bool empty() const {
		return count() == 0;
	}

	size_t capacity() const { return ksize; }
	
	pointer_t direct_access( size_t idx ) { return _data[idx]; }
	const_pointer_t direct_access( size_t idx ) const { return _data[idx]; }

protected:

	datavect_t _data;
	datavect_t _free;
	datavect_t _used;

};

} //namespace ork{
