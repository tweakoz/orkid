////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

/**
 * STL allocator that wraps new.
 *
 * This allocator is zone aware. By default, the memory allocated from is the current zone (-1).
 * Otherwise, it will be the specified Zone.
 */
template <typename T> class allocator {
public:
  // type definitions
  typedef T value_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  // rebind allocator to type U
  template <class U> struct rebind { typedef allocator<U> other; };

  // return address of values
  pointer address(reference value) const { return &value; }
  const_pointer address(const_reference value) const { return &value; }

  /* constructors and destructor
   * - nothing to do because the allocator has no state
   */
  allocator() /*throw()*/ {}

  allocator(const allocator&) /*throw()*/ {}

  template <class U> allocator(const allocator<U>&) /*throw()*/ {}

  ~allocator() /*throw()*/ {}

  // return maximum number of elements that can be allocated
  size_type max_size() const /*throw()*/ { return 0x7FFFFFFFUL / sizeof(value_type); }

  // allocate but don't initialize num elements of type T
  pointer allocate(size_type num, const void* = 0) {
    return static_cast<pointer>(static_cast<void*>(new char[num * sizeof(value_type)]));
  }

  // initialize elements of allocated storage p with value value
  void construct(pointer p, const_reference value) {
    // initialize memory with placement new
    new (p) value_type(value);
  }

  // destroy elements of initialized storage p
  void destroy(pointer p) {
    // destroy objects by calling their destructor
    p->~value_type();
  }

  // deallocate storage p of deleted elements
  void deallocate(pointer p, size_type num) { delete[] static_cast<char*>(static_cast<void*>(p)); }
};

///////////////////////////////////////////////////////////////////////////////

/**
 * STL allocator that wraps new and assumes that no deallocations will occur.
 *
 * A typical usage of this allocator is for global STL containers whose maximum
 * allocation size is known and specified. Global declaration:
 *
 *   vector<CObject*, allocator_alloc_only<CObject*> > gvAllocatedObjects;
 *
 * At one-time init:
 *
 *   gvobjAllocatedObjects.reserve(1024);
 *
 * Another typical usage is for global STL containers that grow to a constant size,
 * but then are never changed or destroyed. Global declaration:
 *
 *   ork::map<std::string, Class*, std::less<string>,
 *     allocator_alloc_only<std::pair<std::string, Class*> >,
 *     allocator_alloc_only<ork::tree_node<std::string, Class*> > > gmClassesByName;
 *
 * At every name to class registration:
 *
 *   std::string strClassName = ...;
 *   Class* pClass = ...;
 *   gmClassesByName[strClassName] = pClass;
 */
template <typename T> class allocator_alloc_only {
public:
  // type definitions
  typedef T value_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  // rebind allocator to type U
  template <class U> struct rebind { typedef allocator_alloc_only<U> other; };

  // return address of values
  pointer address(reference value) const { return &value; }
  const_pointer address(const_reference value) const { return &value; }

  /* constructors and destructor
   * - nothing to do because the allocator has no state
   */
  allocator_alloc_only() /*throw()*/ {}

  allocator_alloc_only(const allocator_alloc_only&) /*throw()*/ {}

  template <class U> allocator_alloc_only(const allocator_alloc_only<U>&) /*throw()*/ {}

  ~allocator_alloc_only() /*throw()*/ {}

  // return maximum number of elements that can be allocated
  size_type max_size() const /*throw()*/ { return 0x7FFFFFFFUL / sizeof(value_type); }

  // allocate but don't initialize num elements of type T
  pointer allocate(size_type num, const void* = 0) {
    return static_cast<pointer>(static_cast<void*>(new char[num * sizeof(value_type)]));
  }

  // initialize elements of allocated storage p with value value
  void construct(pointer p, const_reference value) {
    // initialize memory with placement new
    new (p) value_type(value);
  }

  // destroy elements of initialized storage p
  void destroy(pointer p) {
    // destroy objects by calling their destructor
    p->~value_type();
  }

  // deallocate storage p of deleted elements
  void deallocate(pointer p, size_type num) { delete[] static_cast<char*>(static_cast<void*>(p)); }
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class allocator_stack {
public:
  // type definitions
  typedef T value_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  // rebind allocator to type U
  template <class U> struct rebind { typedef allocator_stack<U> other; };

  // return address of values
  pointer address(reference value) const { return &value; }
  const_pointer address(const_reference value) const { return &value; }

  /* constructors and destructor
   * - nothing to do because the allocator has no state
   */
  allocator_stack() /*throw()*/ {}

  allocator_stack(const allocator_stack&) /*throw()*/ {}

  template <class U> allocator_stack(const allocator_stack<U>&) /*throw()*/ {}

  ~allocator_stack() /*throw()*/ {}

  // return maximum number of elements that can be allocated
  size_type max_size() const /*throw()*/ { return 0x7FFFFFFFUL / sizeof(value_type); }

  // allocate but don't initialize num elements of type T
  pointer allocate(size_type num, const void* = 0) {
    return static_cast<pointer>(static_cast<void*>(new char[num * sizeof(value_type)]));
  }

  // initialize elements of allocated storage p with value value
  void construct(pointer p, const_reference value) {
    // initialize memory with placement new
    new (p) value_type(value);
  }

  // destroy elements of initialized storage p
  void destroy(pointer p) {
    // destroy objects by calling their destructor
    p->~value_type();
  }

  // deallocate storage p of deleted elements
  void deallocate(pointer p, size_type num) {}
};

///////////////////////////////////////////////////////////////////////////////

#if 0
template <typename T>
class allocator_tempstack
{
public:

	// type definitions
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;

	// rebind allocator to type U
	template <class U>
	struct rebind {
		typedef allocator_tempstack<U> other;
	};

	// return address of values
	pointer address(reference value) const {
		return &value;
	}
	const_pointer address(const_reference value) const {
		return &value;
	}

	/* constructors and destructor
	* - nothing to do because the allocator has no state
	*/
	allocator_tempstack() /*throw()*/ {}

	allocator_tempstack(const allocator_tempstack&) /*throw()*/ {}

	template <class U>
	allocator_tempstack(const allocator_tempstack<U>&) /*throw()*/ {}

	~allocator_tempstack() /*throw()*/ {}

	// return maximum number of elements that can be allocated
	size_type max_size() const /*throw()*/ {
		return 0x7FFFFFFFUL / sizeof(value_type);
	}

	// allocate but don't initialize num elements of type T
	pointer allocate(size_type num, const void* = 0) {
		return static_cast<pointer>(static_cast<void *>(OrkTempStackNew char[num*sizeof(value_type)]));
	}

	// initialize elements of allocated storage p with value value
	void construct(pointer p, const_reference value) {
		// initialize memory with placement new
		new(p) value_type(value);
	}

	// destroy elements of initialized storage p
	void destroy(pointer p) {
		// destroy objects by calling their destructor
		p->~value_type();
	}

	// deallocate storage p of deleted elements
	void deallocate(pointer p, size_type num) {
	}

};
#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
