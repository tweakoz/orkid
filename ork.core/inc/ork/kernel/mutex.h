////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orkstd.h>
#include <ork/orkprotos.h>

///////////////////////////////////////////////////////////////////////////////
#include <condition_variable>
#include <mutex>
#include <atomic>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{
	class recursive_mutex
	{
	public:
		recursive_mutex( const char* name )
			: mTheMutex()
			, mName(name)
			, miLockCount(0)
		{
		}
		void Lock(int lid=-1)
		{
			mTheMutex.lock();
			mLockIDs.push(lid);
			miLockCount++;
		}
		void UnLock()
		{
			miLockCount--;
			mLockIDs.pop();
			mTheMutex.unlock();
		}
		bool TryLock()
		{
			bool bv = mTheMutex.try_lock();
			if( bv )
			{
				mLockIDs.push(-2);
				miLockCount++;
			}
			return bv;
		}
		int GetLockCount() const { return miLockCount; }
		typedef std::unique_lock<std::recursive_mutex> recursive_scoped_lock;
	private:
		std::recursive_mutex	mTheMutex;
		std::string mName;
		std::stack<int> mLockIDs;
		int miLockCount;
	};
	class mutex
	{
	public:

		typedef std::mutex mutex_impl_t;

		mutex( const char* name ) 
			: mTheMutex()
			, mName(name)
			, miLockID(-1)
		{
		}
		void Lock(int lid=-1)
		{
			mTheMutex.lock();
			miLockID = lid;
		}
		void UnLock()
		{
			mTheMutex.unlock();
		}
		bool TryLock()
		{
			return mTheMutex.try_lock();
		}

		struct unique_lock
		{
			typedef std::unique_lock<mutex_impl_t> lock_impl_t;
			unique_lock(ork::mutex& mtx) : mLockImpl(mtx.mTheMutex) {}
			~unique_lock() {}
			lock_impl_t mLockImpl;
		};

		//typedef tbb::mutex::scoped_lock scoped_lock;
	private:
		mutex_impl_t mTheMutex;
		std::string mName;
		int miLockID;
	};

};

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <typename T> class LockedResource
{
	T												mResource;
	mutable ork::recursive_mutex					mResourceMutex;

public:
	LockedResource( const char* pname = "ResourceMutex" )
		: mResourceMutex( pname )
	{
	}
	LockedResource( const LockedResource& oth )
		: mResourceMutex( oth.mResourceMutex.mname )
	{
	}
	~LockedResource()
	{
		mResourceMutex.Lock();
		mResourceMutex.UnLock();
	}	

	const T GetByValueLocked(int lid=-1) const
	{
		mResourceMutex.Lock(lid);
		T result_by_value = mResource;
		mResourceMutex.UnLock();
		return result_by_value;
	}

	const T& LockForRead(int lid=-1) const
	{
		mResourceMutex.Lock(lid);
		return mResource;
	}
	T& LockForWrite(int lid=-1)
	{
		mResourceMutex.Lock(lid);
		return mResource;
	}
	void UnLock() const
	{
		mResourceMutex.UnLock();
	}
	int GetLockCount() const 
	{
		return mResourceMutex.GetLockCount();
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct dummy_critsect
{
	const char* mname;

	dummy_critsect( const char* name )
		: mname( name )
	{
	}
	~dummy_critsect()
	{
	}

	// for trylock TryEnterCriticalSection

	class standard_lock
	{
	public:

		dummy_critsect & cs;
		bool	mblocked;

		standard_lock( dummy_critsect& c )
			: cs( c )
			, mblocked( false )
		{

		}
		
		~standard_lock()
		{
			OrkAssert( mblocked == false );
		}

		void Lock( void )
		{
			mblocked = true;
		}

		void UnLock( void )
		{
			mblocked = false;
		}

		bool IsLocked( void ) const
		{
			return mblocked;
		}

	private:

		static dummy_critsect gcs;

		standard_lock( const standard_lock& oth )
			: cs(gcs)
		{
			OrkAssert(false);
		}

	};
};

struct dummy_mutex
{
	const char* mname;

	dummy_mutex(const char *name)
		: mname(name)
	{
	}

	~dummy_mutex()
	{
	}

	struct standard_lock
	{
		dummy_mutex & mMutex;
		bool	mblocked;

		standard_lock( dummy_mutex& mtx )
			: mMutex( mtx )
			, mblocked( false )
		{
			//orkprintf( "standard_lock() Mutex<%08x>\n", & mMutex );
		}

		~standard_lock()
		{
			//orkprintf( "~standard_lock() Mutex<%08x>\n", & mMutex );
			OrkAssert( mblocked == false );
		}

		void Lock( void )
		{
			mblocked = true;
		}

		void UnLock( void )
		{
			mblocked = false;
		}

		bool IsLocked( void ) const
		{
			return mblocked;
		}
	};

	struct scoped_lock
	{
		dummy_mutex & mMutex;
		bool	mblocked;

		scoped_lock( dummy_mutex & mtx )
			: mMutex( mtx )
			, mblocked( true )
		{
		}

		~scoped_lock()
		{
		}

		bool IsLocked( void ) const
		{
			return mblocked;
		}
	};

	struct try_lock
	{
		dummy_mutex & mMutex;
		bool	mblocked;

		try_lock( dummy_mutex & mtx )
			: mMutex( mtx )
			, mblocked( false )
		{
		
		}

		bool lock()
		{
			bool bret = mblocked==false;
			mblocked = true;
			return bret;
		}

		void unlock()
		{
			OrkAssert( mblocked );
			mblocked = false;
		}

		~try_lock()
		{
			if( mblocked )
			{
				unlock();
			}
		}

		bool IsLocked( void ) const
		{
			return mblocked;
		}
	};
};
	
}
///////////////////////////////////////////////////////////////////////////////

