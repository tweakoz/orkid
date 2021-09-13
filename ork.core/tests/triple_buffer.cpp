#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/util/triple_buffer.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/thread.h>

using namespace ork;

typedef ork::FixedString<256> extstring_t;

struct YOTEST {
  svar4096_t mVar;
  YOTEST(int i) {
  }
};

static const int knummsgs    = 4 << 10;
static const float ktesttime = 5.0f;

template <typename queue_type> struct yo {
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
      , mConsGot(0) {
  }

  void RunTest() {

    auto l_producer = [&](anyp inp) {
      auto pyo              = inp.get<yo*>();
      queue_type& the_queue = pyo->mQueue;
      Timer tm;
      Timer tm2;
      tm.Start();
      tm2.Start();
      int i = 0;

      while (tm.SecsSinceStart() < ktesttime) {
        auto pv = the_queue.begin_push();
        pyo->mProdCounter++;
        if (pv) {
          pyo->mProdGot++;
          extstring_t str;
          str.format("to<%d>", i);
          pv->mVar.template set(str);
          // usleep(rand()%3);
        }
        the_queue.end_push(pv);
        if (tm2.SecsSinceStart() > 1.0f) {
          printf("push ictr<%d> igot<%d> delt<%d>\n", pyo->mProdCounter, pyo->mProdGot, (pyo->mProdCounter - pyo->mProdGot));
          tm2.Start();
        }
      }
    };

    auto l_consumer = [&](anyp inp) {
      auto pyo              = inp.get<yo*>();
      queue_type& the_queue = pyo->mQueue;

      Timer tm;
      Timer tm2;
      tm.Start();
      tm2.Start();

      while (tm.SecsSinceStart() < ktesttime) {
        auto pv = the_queue.begin_pull();
        pyo->mConsCounter++;
        if (pv) {
          pyo->mConsGot++;
          auto str = pv->mVar.template get<extstring_t>();
          // usleep(rand()%3);
        }
        the_queue.end_pull(pv);

        if (tm2.SecsSinceStart() > 1.0f) {
          printf("pull ictr<%d> igot<%d> delt<%d>\n", pyo->mConsCounter, pyo->mConsGot, (pyo->mConsCounter - pyo->mConsGot));
          tm2.Start();
        }
      }
    };

    double fsynctime = ork::get_sync_time();

    ork::Thread thr_p(l_producer, this);
    ork::Thread thr_c(l_consumer, this);

    thr_p.join();
    thr_c.join();

    double fsynctime2 = ork::get_sync_time();

    double elapsed  = (fsynctime2 - fsynctime);
    double mps      = double(mConsGot) / elapsed;
    double msg_size = double(sizeof(value_type));
    double bps      = msg_size * mps;
    double gbps     = bps / double(1 << 30);
    printf("nummsgs<%g> elapsed<%g> msgpersec<%g> GBps(%g)\n", double(mConsGot), elapsed, mps, gbps);
  }
};
///////////////////////////////////////////////////////////////////////////////

TEST(triplebuffer) {
  printf("//////////////////////////////////////\n");
  printf("ORK TRIPLEBUF TEST\n");
  printf("//////////////////////////////////////\n");

  typedef concurrent_triple_buffer<YOTEST> tb_type;
  auto the_yo = new yo<tb_type>;
  the_yo->RunTest();
  delete the_yo;

  printf("//////////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////
