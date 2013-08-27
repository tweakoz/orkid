////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orkstd.h>
#include <ork/orkprotos.h>

#if defined(ORK_VS2012) // builtin mutex
#define USE_STD_MUTEX
#elif defined(ORK_OSX)
//#define USE_TBB_MUTEX
#define USE_STD_MUTEX
#else
#define USE_TBB_MUTEX
//#define USE_STD_MUTEX
#endif

#include <ork/kernel/atomic.h>

///////////////////////////////////////////////////////////////////////////////
#if defined(USE_STD_MUTEX)
#include <condition_variable>
#include <mutex>
#include <atomic>
#elif defined(USE_TBB_MUTEX)
#include <tbb/compat/condition_variable>
#include <tbb/mutex.h>
#include <tbb/recursive_mutex.h>
namespace std
{
    typedef tbb::mutex mutex;
#if defined(ORK_OSX)
	template <typename T> using unique_lock = tbb::interface5::unique_lock<T>;
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////

#if defined(USE_STD_MUTEX)

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

#elif defined(USE_TBB_MUTEX)

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
		typedef tbb::recursive_mutex::scoped_lock recursive_scoped_lock;
	private:
		tbb::recursive_mutex	mTheMutex;
		std::string mName;
		std::stack<int> mLockIDs;
		int miLockCount;
	};
	class mutex
	{
	public:

		typedef tbb::mutex mutex_impl_t;

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
		struct scoped_lock
		{
			typedef mutex_impl_t::scoped_lock lock_impl_t;

			scoped_lock(ork::mutex& mtx) : mLockImpl(mtx.mTheMutex) {}
			~scoped_lock() {}


			lock_impl_t mLockImpl;

		};

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

#elif defined( WII )

#include <revolution/os.h>

namespace ork
{
	struct critsect
	{
		OSMutex cs;
		const char* mname;

		critsect( const char* name )
			: mname( name )
		{
			OSInitMutex (&cs);
		}
		~critsect()
		{
		}

		// for trylock TryEnterCriticalSection

		struct standard_lock
		{
			critsect & cs;
			bool	mblocked;

			standard_lock( critsect& c )
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
				OSLockMutex(&cs.cs);
				mblocked = true;
			}

			void UnLock( void )
			{
				OSUnlockMutex (&cs.cs);
				mblocked = false;
			}

			bool IsLocked( void ) const
			{
				return mblocked;
			}

		};
	};

	struct mutex
	{
		OSMutex mMutex;
		const char* mname;

		mutex(const char *name)
			: mname(name)
		{
			OSInitMutex (&mMutex);
		}

		~mutex()
		{
		}
	
		struct standard_lock
		{
			mutex & mMutex;
			bool	mblocked;

			standard_lock( mutex& mtx )
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
				OSLockMutex(&mMutex.mMutex);
				mblocked = true;
			}

			void UnLock( void )
			{
				OSUnlockMutex (&mMutex.mMutex);
				mblocked = false;
			}

			bool IsLocked( void ) const
			{
				return mblocked;
			}
		};

		struct scoped_lock
		{
			mutex & mMutex;
			bool	mblocked;

			scoped_lock( mutex & mtx )
				: mMutex( mtx )
				, mblocked( true )
			{
				OSLockMutex(&mMutex.mMutex);
				//orkprintf( "Lock Mutex<%08x> ret<%08x>\n", & mMutex, dw );
				//OrkAssert( iresult == SCE_KERNEL_ERROR_OK, "cannot aquire semaphore" );
			}

			~scoped_lock()
			{
				OSUnlockMutex (&mMutex.mMutex);
				//OrkAssert( iresult == SCE_KERNEL_ERROR_OK, "cannot return semaphore" );
			}

			bool IsLocked( void ) const
			{
				return mblocked;
			}
		};

		struct try_lock
		{
			mutex & mMutex;
			bool	mblocked;

			try_lock( mutex & mtx )
				: mMutex( mtx )
				, mblocked( false )
			{
			
			}

			bool lock()
			{
				BOOL bret = OSTryLockMutex(&mMutex.mMutex);
				mblocked = ( bret == TRUE );
				return mblocked;
			}

			void unlock()
			{
				OrkAssert( mblocked );
				OSUnlockMutex (&mMutex.mMutex);
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

};

#elif defined( _PSP )

#include <kernel.h>
#include <kerror.h>
#include <iofilemgr.h>
#include <psptypes.h>

namespace ork
{
	struct mutex
	{
		mutex(const char *name)
			: mSemaphore( sceKernelCreateSema( name, SCE_KERNEL_SA_THFIFO, 1, 1, NULL ) )
		{
		}

		~mutex()
		{
			sceKernelDeleteSema( mSemaphore );
		}

		SceUID mSemaphore;

	
		struct standard_lock
		{
			mutex * mpMutex;
			bool	mblocked;

			standard_lock( mutex * mtx = 0 )
				: mpMutex( mtx )
				, mblocked( false )
			{}

			void Lock( void )
			{
				int iresult = sceKernelWaitSema( mpMutex->mSemaphore, 1, NULL );
				OrkAssertI( iresult == SCE_KERNEL_ERROR_OK, "cannot aquire semaphore" );
			}

			void UnLock( void )
			{
				int iresult = sceKernelSignalSema( mpMutex->mSemaphore, 1 );
				OrkAssertI( iresult == SCE_KERNEL_ERROR_OK, "cannot return semaphore" );
			}

			bool IsLocked( void ) const
			{
				return mblocked;
			}
		};

		struct scoped_lock
		{
			mutex & mMutex;
			bool	mblocked;

			scoped_lock( mutex & mtx )
				: mMutex( mtx )
				, mblocked( true )
			{
				int iresult = sceKernelWaitSema( mMutex.mSemaphore, 1, NULL );
				OrkAssertI( iresult == SCE_KERNEL_ERROR_OK, "cannot aquire semaphore" );
			}

			~scoped_lock()
			{
				int iresult = sceKernelSignalSema( mMutex.mSemaphore, 1 );
				OrkAssertI( iresult == SCE_KERNEL_ERROR_OK, "cannot return semaphore" );
			}

			bool locked( void ) const
			{
				return mblocked;
			}
		};

		struct try_lock
		{
			mutex & mMutex;
			bool	mblocked;

			try_lock( mutex & mtx )
				: mMutex( mtx )
				, mblocked( false )
			{
			
				int iresult = sceKernelPollSema( mMutex.mSemaphore, 1 );
				mblocked = ( iresult == SCE_KERNEL_ERROR_OK );
			}

			~try_lock()
			{
				if( mblocked )
				{
					int iresult = sceKernelSignalSema( mMutex.mSemaphore, 1 );
					OrkAssertI( iresult == SCE_KERNEL_ERROR_OK, "cannot return semaphore" );
				}
			}

			bool IsLocked( void ) const
			{
				return mblocked;
			}
		};
	};

	typedef mutex critsect; // wii doesnt have "processes" only threads

};

#else
#if defined(_DARWIN)
#include <unistd.h>
#endif

#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>

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
	private:
		boost::recursive_mutex	mTheMutex;
		std::string mName;
		std::stack<int> mLockIDs;
		int miLockCount;
	};
	class mutex
	{
	public:
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
	private:
		boost::mutex mTheMutex;
		std::string mName;
		int miLockID;
	};
	typedef boost::mutex::scoped_lock scoped_lock;
	typedef boost::recursive_mutex::scoped_lock recursive_scoped_lock;

};

#endif

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

