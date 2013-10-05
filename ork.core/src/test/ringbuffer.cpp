#include <unittest++/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/kernel/ringbuffer.hpp>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>

using namespace ork;

typedef pthread_t thread_t;
typedef ork::FixedString<256> extstring_t;

void thread_start(thread_t* th, void*(*func)(void*), void* arg)
{
    pthread_create(th, 0, func, arg);
}


void thread_join(thread_t* th)
{
    void*                       tmp;
    pthread_join(th[0], &tmp);
}

struct EOTEST
{

};

static const int knummsgs = 4<<10;

template <typename queue_type>
	struct yo 
	{
		typedef typename queue_type::value_type value_type;

		queue_type mQueue;

		void RunTest()
		{
			double fsynctime = ork::get_sync_time();
			thread_t thr_p, thr_c;
		    thread_start(&thr_p, producer_thread, (void*)this);
		    thread_start(&thr_c, consumer_thread, (void*)this);

		    thread_join(&thr_p);
		    thread_join(&thr_c);
			double fsynctime2 = ork::get_sync_time();

			double elapsed = (fsynctime2-fsynctime);
			double mps = 2.0*double(knummsgs)/elapsed;
			double msg_size = double(sizeof(value_type));
			double bps = msg_size*mps;
			double gbps = bps/double(1<<30);
			printf( "nummsgs<%g> elapsed<%g> msgpersec<%g> GBps(%g)\n", double(knummsgs), elapsed, mps, gbps );			
		}

		static void* producer_thread(void* ctx)
		{
			yo* pyo = (yo*) ctx;
			queue_type& the_queue = pyo->mQueue;
			for( int i=0; i<knummsgs; i++ )
			{
				the_queue.push(i);
				extstring_t str;
				str.format("to<%d>", i);
				the_queue.push(str);
			}
			the_queue.push(EOTEST());
			return 0;
		}
		static void* consumer_thread(void* ctx)
		{
			yo* pyo = (yo*) ctx;
			queue_type& the_queue = pyo->mQueue;
			value_type popped;

			bool bdone=false;
			int ictr1 = 0;
			int ictr2 = 0;
			while(false==bdone)
			{
				while( the_queue.try_pop(popped) )
				{
					if( popped.template IsA<int>() )
					{
						int iget = popped.template Get<int>();
						assert(iget==ictr1);
						ictr1++;
					}
					else if( popped.template IsA<extstring_t>() )
					{
						extstring_t& es = popped.template Get<extstring_t>();
						if( (ictr2%(1<<18))==0 )
							printf( "popped: %s\n", es.c_str() );
						ictr2++;
						assert(ictr2==ictr1);
					}
					else if( popped.template IsA<EOTEST>() )
						bdone=true;
					else
						assert(false);
				}
				usleep(1000);
			}
			return 0;
		}



	};

///////////////////////////////////////////////////////////////////////////////

TEST(OrkMpMcRingBuf)
{
	printf("//////////////////////////////////////\n" );
	printf( "ORK MPMC CONQ TEST\n");
	printf("//////////////////////////////////////\n" );

	//auto the_yo = new yo<ork::mpmc_bounded_queue<svar1024_t,16<<10>>;
	auto the_yo = new yo<ork::MpMcRingBuf<svar4096_t,16<<10>>;
	the_yo->RunTest();
	delete the_yo;

	printf("//////////////////////////////////////\n" );

}

///////////////////////////////////////////////////////////////////////////////
