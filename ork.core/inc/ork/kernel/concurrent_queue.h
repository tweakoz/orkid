///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#define _USE_TBB

#include <ork/kernel/ringbuffer.hpp>
#if defined(WIN32)
#include <concurrent_queue.h>
//typedef Concurrency::concurrent_queue<shared_op_ptr_t> opq_t;
#elif defined(_USE_TBB)
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_vector.h>
#include <lockfree/lockfree/fifo.hpp>
//#include <amino/queue.h>
#elif defined(_DARWIN) // GCD based 
#include <dispatch/dispatch.h>
#include <ork/kernel/string/string.h>
#include <lockfree/lockfree/fifo.hpp>
#else // generic builtin
#define GENERIC_CONQ
#include <ork/kernel/mutex.h>
#include <queue>
#endif
namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <typename T,size_t max_items=256>
struct MpMcBoundedQueue
{
    typedef MpMcRingBuf<T,max_items> impl_t;
    typedef T value_type;

    MpMcBoundedQueue()
		: mImpl()
    {

    }
    ~MpMcBoundedQueue()
    {

    }
    void push(const T& item) // blocking
    {
        mImpl.push(item);
    }
    bool try_push(const T& item) // non-blocking
    {
        return mImpl.try_push(item);
    }
    bool try_pop(T& item) // non-blocking
    {
        return mImpl.try_pop(item);
    }

    impl_t mImpl;
    static const size_t kSIZE = sizeof(T);
};

///////////////////////////////////////////////////////////////////////////////

#if defined(WIN32)
template <typename T> class ConcurrentQueue
{
public:

	void push( const T& item )
	{
		mQueue.push( item );
	}

	void pop( T& output_item )
	{
		mQueue.pop( output_item );
	}

	bool try_pop( T& output_item )
	{
		return mQueue.try_pop( output_item );
	}
	ConcurrentQueue( int isize ) : mQueue(isize) {}
	ConcurrentQueue() : mQueue() {}

private:

	Concurrency::concurrent_queue<T> mQueue;

};
#elif defined(_USE_TBB)
template <typename T> class ConcurrentQueue
{
public:

	void push( const T& item )
	{
		mQueue.push( item );
	}

	void pop( T& output_item )
	{
		mQueue.pop( output_item );
	}
	bool try_pop( T& output_item )
	{
		return mQueue.try_pop( output_item );
	}

private:

	tbb::concurrent_bounded_queue<T>	mQueue;

};
template <typename T> class ConcurrentFixedQueue
{
public:

	ConcurrentFixedQueue( int isize )
	{
		mQueue.set_capacity(isize);
	}

	void push( const T& item )
	{
		mQueue.push( item );
	}

	void pop( T& output_item )
	{
		mQueue.pop( output_item );
	}

	bool try_pop( T& output_item )
	{
		return mQueue.try_pop( output_item );
	}

	int size() const 
	{
		int isize = mQueue.size();
		return isize;
	}

	bool empty() const
	{
		int isize = mQueue.size();

		bool bempty = (isize<=0);

		return bempty;
	}

	void threadunsafe_clear() 
	{
		mQueue.clear();
	}

private:

	tbb::concurrent_bounded_queue<T>	mQueue;

};
template <typename T> class ConcurrentVector
{
public:

	ConcurrentVector()
	{
	}

	void push_back( const T& item )
	{
		mVector.push_back( item );
	}

	int size() const 
	{
		int isize = mVector.size();
		return isize;
	}

	bool empty() const
	{
		int isize = mVector.size();

		bool bempty = (isize<=0);

		return bempty;
	}

	const T& get_item(const int index) const
	{
		return mVector[index];
	}

	void threadunsafe_clear() 
	{
		mVector.clear();
	}

private:

	tbb::concurrent_vector<T>	mVector;

};
#elif defined(_DARWIN)
template<typename T>
class ConcurrentQueue
{
public:
    void push(const T& item)
    {
		the_queue.enqueue(item);
    }

    bool try_pop(T& popped_value)
    {
		return the_queue.dequeue(popped_value);
    }

    void pop(T& popped_value)
    {
		bool bpopped = false;
		while( false==bpopped )
		{
			bpopped = the_queue.dequeue(popped_value);
		}
    }
	ConcurrentQueue( int isize ) : the_queue(isize) {}
	ConcurrentQueue() : the_queue() {}
	
private:
    boost::lockfree::fifo<T> the_queue;
};
#elif defined(GENERIC_CONQ)
template <typename T> class ConcurrentQueue
{
public:

	void push( const T& item )
	{
		std::queue<T>& q = mQueue.LockForWrite();
		q.push( item );
		mQueue.UnLock();
	}

	void pop( T& output_item )
	{
		bool bwhile = true;
		while( bwhile )
		{
			std::queue<T>& q = mQueue.LockForWrite();
			if( q.empty() == false )
			{
				output_item = q.pop();
				bwhile = false;
			}
			mQueue.UnLock;
		}
	}

	bool try_pop( T& output_item )
	{
		bool brval = false;
		bool bwhile = true;
		while( bwhile )
		{
			std::queue<T>& q = mQueue.LockForWrite();
			if( q.empty() == false )
			{
				output_item = q.front();
				q.pop();
				bwhile = false;
				brval = true;
			}
			mQueue.UnLock();
		}
		return brval;
	}

private:

	ork::LockedResource< std::queue<T> >	mQueue;

};
template <typename T> class ConcurrentFixedQueue
{
public:

	ConcurrentFixedQueue( int isize )
	{
		//mQueue.set_capacity(isize);
	}

	void push_blocking( const T& item )
	{
		std::queue<T>& q = mQueue.LockForWrite();
		q.push(item);
		mQueue.UnLock();
	}

	void pop_blocking( T& output_item )
	{
		bool bwhile = true;
		while( bwhile )
		{
			std::queue<T>& q = mQueue.LockForWrite();
			if( q.empty() == false )
			{
				output_item = q.pop();
				bwhile = false;
			}
			mQueue.UnLock;
		}
	}

	bool pop_nonblock( T& output_item )
	{
		bool bret = false;
		std::queue<T>& q = mQueue.LockForWrite();
		if( q.empty() == false )
		{
			output_item = q.front();
			q.pop();
			bret = true;
		}
		mQueue.UnLock();
		return bret;
	}

	int size() const 
	{
		std::queue<T>& q = mQueue.LockForWrite();
		int iret = q.size();
		mQueue.UnLock();
		return iret;
	}

	bool empty() const
	{
		std::queue<T>& q = mQueue.LockForWrite();
		bool bret = q.empty();
		mQueue.UnLock();
		return bret;
	}

	void threadunsafe_clear() 
	{
		std::queue<T>& q = mQueue.LockForWrite();
		q.clear();
		mQueue.UnLock();
	}

private:

	ork::LockedResource< std::queue<T> >	mQueue;

};
#endif

///////////////////////////////////////////////////////////////////////////////

}
