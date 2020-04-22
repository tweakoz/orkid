#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <memory>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>
#include <string.h>

#include <ork/util/multi_buffer.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/thread.h>

using namespace ork;

typedef pthread_t thread_t;
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

  queue_type mQueue01;
  queue_type mQueue12;

  int mStage0Pushed;
  int mStage1Pulled;
  int mStage1Pushed;
  int mStage2Pulled;

  ork::atomic<bool> mStage0ExitPlease;
  ork::atomic<bool> mStage1ExitPlease;
  ork::atomic<bool> mStage2ExitPlease;

  yo()
      : mStage0Pushed(0)
      , mStage1Pulled(0)
      , mStage1Pushed(0)
      , mStage2Pulled(0) {
    mStage0ExitPlease = false;
    mStage1ExitPlease = false;
    mStage2ExitPlease = false;
  }

  void Stage0() {
    int i = 0;

    while (false == mStage0ExitPlease) {
      auto pv = mQueue01.BeginWrite();
      OrkAssert(pv);
      mStage0Pushed++;
      extstring_t str;
      str.format("to<%d>", i);
      pv->mVar.template Set(str);
      // usleep(rand()%3);
      mQueue01.EndWrite(pv);
    }
    printf("QSIZE<%d> Stage0 Exiting....\n", queue_type::knumitems);
  }
  void Stage1() // thr
  {
    while (false == mStage1ExitPlease) {
      ////////////////////////////////////////
      auto pv01 = mQueue01.BeginRead();

      if (pv01) {
        mStage1Pulled++;
        auto str = pv01->mVar.template Get<extstring_t>();
        mQueue01.EndRead(pv01);
        ////////////////////////////////////////
        // pass it on
        ////////////////////////////////////////
        auto pv12 = mQueue12.BeginWrite();
        OrkAssert(pv12);
        mStage1Pushed++;
        pv12->mVar.template Set(str);
        mQueue12.EndWrite(pv12);
        ////////////////////////////////////////
      } else if (mStage1ExitPlease)
        return;
      else
        usleep(queue_type::kquanta);
    }

    printf("QSIZE<%d> Stage1 Exiting....\n", queue_type::knumitems);
  }
  void Stage2() // thr
  {
    Timer tm2;
    tm2.Start();

    while (false == mStage2ExitPlease) {
      auto pv12 = mQueue12.BeginRead();

      if (pv12) {
        mStage2Pulled++;
        auto str = pv12->mVar.template Get<extstring_t>();
        mQueue12.EndRead(pv12);

        if (tm2.SecsSinceStart() > 1.0f) {
          printf(
              "QSIZE<%d> stg0-pushed<%d> stg1-pulled<%d> stg2-pulled<%d>\n",
              queue_type::knumitems,
              mStage0Pushed,
              mStage1Pulled,
              mStage2Pulled);
          tm2.Start();
        }
      } else if (mStage2ExitPlease)
        return;
      else
        usleep(queue_type::kquanta);
    }

    printf("QSIZE<%d> Stage2 Exiting....\n", queue_type::knumitems);
  }
  void RunTest() {
    printf("/////////////////////////////////////////////////\n");

    ork::Thread stage0("stage0"), stage1("stage1"), stage2("stage2");

    Timer tm;
    tm.Start();

    stage0.start([=](anyp data) { this->Stage0(); });
    stage1.start([=](anyp data) { this->Stage1(); });
    stage2.start([=](anyp data) { this->Stage2(); });

    usleep(4 << 20);
    this->mStage0ExitPlease = true;
    stage0.join();
    this->mStage1ExitPlease = true;
    stage1.join();
    this->mStage2ExitPlease = true;
    stage2.join();

    double elapsed  = tm.SecsSinceStart();
    double mps      = double(mStage2Pulled) / elapsed;
    double msg_size = double(sizeof(value_type));
    double bps      = msg_size * mps;
    double gbps     = bps / double(1 << 30);
    printf(
        "QSIZE<%d> nummsgs<%d> elapsed<%g> msgpersec<%g> GBps(%g)\n",
        queue_type::knumitems,
        int(mStage2Pulled),
        elapsed,
        mps,
        gbps);
    printf("/////////////////////////////////////////////////\n");
  }
};

///////////////////////////////////////////////////////////////////////////////

TEST(MultiBufTest) {
  printf("//////////////////////////////////////\n");
  printf("ORK MultiBufTest TEST\n");
  printf("//////////////////////////////////////\n");

  {
    using yotype_t = yo<concurrent_multi_buffer<YOTEST, 8192>>;
    auto the_yo    = std::make_shared<yotype_t>();
    the_yo->RunTest();
  }
  {
    using yotype_t = yo<concurrent_multi_buffer<YOTEST, 1024>>;
    auto the_yo    = std::make_shared<yotype_t>();
    the_yo->RunTest();
  }
  {
    using yotype_t = yo<concurrent_multi_buffer<YOTEST, 128>>;
    auto the_yo    = std::make_shared<yotype_t>();
    the_yo->RunTest();
  }
  {
    using yotype_t = yo<concurrent_multi_buffer<YOTEST, 16>>;
    auto the_yo    = std::make_shared<yotype_t>();
    the_yo->RunTest();
  }
  {
    using yotype_t = yo<concurrent_multi_buffer<YOTEST, 2>>;
    auto the_yo    = std::make_shared<yotype_t>();
    the_yo->RunTest();
  }

  printf("MultiBufTest MainThread Exiting....\n");

  printf("//////////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////
