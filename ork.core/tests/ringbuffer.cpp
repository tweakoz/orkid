#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/kernel/ringbuffer.hpp>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/thread.h>

using namespace ork;
#if 1
typedef ork::Thread thread_t;

struct EOTEST {};

typedef fxstring<16> extstring_t;

static const int knummsgs = 1 << 24;

template <typename queue_type> struct yo {
  typedef typename queue_type::value_type value_type;

  queue_type mQueue;

  void RunTest() {
    auto l_producer = [&](anyp inp) {
      auto pyo              = inp.get<yo*>();
      queue_type& the_queue = pyo->mQueue;
      printf("STARTED PRODUCER knummsgs<%d>\n", knummsgs);
      for (int i = 0; i < knummsgs; i++) {
        // printf( " prod pushing<%d>\n", i );
        the_queue.push(i);
        extstring_t str;
        str.format("to<%d>", i);
        the_queue.push(str);
      }
      the_queue.push(EOTEST());
    };

    auto l_consumer = [&](anyp inp) {
      auto pyo = inp.get<yo*>();
      printf("STARTED CONSUMER\n");
      queue_type& the_queue = pyo->mQueue;
      value_type popped;

      bool bdone = false;
      int ictr1  = 0;
      int ictr2  = 0;
      while (false == bdone) {
        while (the_queue.try_pop(popped)) {
          // printf( " cons pulling ictr1<%d> ictr2<%d>\n", ictr1, ictr2 );

          if (popped.template isA<int>()) {
            int iget = popped.template get<int>();
            // printf( "popped int<%d>\n", iget );
            assert(iget == ictr1);
            ictr1++;
          } else if (popped.template isA<extstring_t>()) {
            extstring_t& es = popped.template get<extstring_t>();
            // if( (ictr2%(1<<18))==0 )
            //	printf( "popped str<%s>\n", es.c_str() );
            ictr2++;
            assert(ictr2 == ictr1);
          } else if (popped.template isA<EOTEST>())
            bdone = true;
          else
            assert(false);
        }
        usleep(2000);
      }
    };

    double fsynctime = ork::get_sync_time();

    ork::Thread thr_p(l_producer, this);
    ork::Thread thr_c(l_consumer, this);

    thr_p.join();
    thr_c.join();

    double fsynctime2 = ork::get_sync_time();

    double elapsed  = (fsynctime2 - fsynctime);
    double mps      = 2.0 * double(knummsgs) / elapsed;
    double msg_size = double(sizeof(value_type));
    double bps      = msg_size * mps;
    double gbps     = bps / double(1 << 30);
    printf("nummsgs<%g> elapsed<%g> msgpersec<%g> GBps(%g)\n", double(knummsgs), elapsed, mps, gbps);
  }
};

///////////////////////////////////////////////////////////////////////////////

TEST(OrkMpMcRingBuf) {
  printf("//////////////////////////////////////\n");
  printf("ORK MPMC CONQ TEST\n");
  printf("//////////////////////////////////////\n");

  // auto the_yo = new yo<ork::mpmc_bounded_queue<svar1024_t,16<<10>>;
  using yotype_t = yo<ork::MpMcRingBuf<svar4096_t, 16 << 10>>;
  auto the_yo    = std::make_shared<yotype_t>();
  the_yo->RunTest();

  printf("//////////////////////////////////////\n");
}
#endif

///////////////////////////////////////////////////////////////////////////////
