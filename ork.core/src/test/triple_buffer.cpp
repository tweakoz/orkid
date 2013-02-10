#include <unittest++/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/util/triple_buffer.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>

using namespace ork;

typedef pthread_t thread_t;
typedef ork::FixedString<256> extstring_t;

void thread_start(thread_t* th, void*(*func)(void*), void* arg);
void thread_join(thread_t* th);

struct YOTEST
{
	svar4096_t mVar;
	YOTEST(int i){}
};

static const int knummsgs = 4<<10;
static const float ktesttime = 60.0f;

template <typename queue_type>
	struct yo 
	{
		typedef typename queue_type::value_type value_type;

		queue_type mQueue;
		int mProdCounter;
		int mConsCounter;
		int mProdGot;
		int mConsGot;

		yo()
			: mProdCounter(0)
			, mConsCounter(0)
			, mProdGot(0)
			, mConsGot(0)
		{

		}

		void RunTest()
		{
			double fsynctime = ork::get_sync_time();
			thread_t thr_p, thr_c;
		    thread_start(&thr_c, consumer_thread, (void*)this);
		    usleep(1000);
		    thread_start(&thr_p, producer_thread, (void*)this);

		    thread_join(&thr_p);
		    thread_join(&thr_c);
			double fsynctime2 = ork::get_sync_time();

			double elapsed = (fsynctime2-fsynctime);
			double mps = double(mConsGot)/elapsed;
			double msg_size = double(sizeof(value_type));
			double bps = msg_size*mps;
			double gbps = bps/double(1<<30);
			printf( "nummsgs<%g> elapsed<%g> msgpersec<%g> GBps(%g)\n", double(mConsGot), elapsed, mps, gbps );			
		}


		static void* producer_thread(void* ctx)
		{
			yo* pyo = (yo*) ctx;
			queue_type& the_queue = pyo->mQueue;
			Timer tm;
			Timer tm2;
			tm.Start();
			tm2.Start();
			int i = 0;

			while( tm.SecsSinceStart()<ktesttime )
			{
				auto pv = the_queue.begin_push();
				pyo->mProdCounter++;
				if( pv )
				{
					pyo->mProdGot++;
					extstring_t str;
					str.format("to<%d>", i);
					pv->mVar.template Set(str);
					//usleep(rand()%3);
				}
				the_queue.end_push(pv);
				if( tm2.SecsSinceStart()>1.0f )
				{
					printf( "push ictr<%d> igot<%d> delt<%d>\n", pyo->mProdCounter, pyo->mProdGot, (pyo->mProdCounter-pyo->mProdGot) );
					tm2.Start();
				}
			}
			return 0;
		}
		static void* consumer_thread(void* ctx)
		{
			yo* pyo = (yo*) ctx;
			queue_type& the_queue = pyo->mQueue;

			Timer tm;
			Timer tm2;
			tm.Start();
			tm2.Start();

			while( tm.SecsSinceStart()<ktesttime )
			{
				auto pv = the_queue.begin_pull();
				pyo->mConsCounter++;
				if( pv )
				{
					pyo->mConsGot++;
					auto str = pv->mVar.template Get<extstring_t>();
					//usleep(rand()%3);
				}
				the_queue.end_pull(pv);

				if( tm2.SecsSinceStart()>1.0f )
				{
					printf( "pull ictr<%d> igot<%d> delt<%d>\n", pyo->mConsCounter, pyo->mConsGot, (pyo->mConsCounter-pyo->mConsGot) );
					tm2.Start();
				}
			}

			return 0;
		}



	};

///////////////////////////////////////////////////////////////////////////////

TEST(OrkTripleBufTest)
{
	printf("//////////////////////////////////////\n" );
	printf( "ORK TRIPLEBUF TEST\n");
	printf("//////////////////////////////////////\n" );

	typedef concurrent_triple_buffer<YOTEST> tb_type;
	auto the_yo = new yo<tb_type>;
	the_yo->RunTest();
	delete the_yo;

	printf("//////////////////////////////////////\n" );

}

///////////////////////////////////////////////////////////////////////////////
