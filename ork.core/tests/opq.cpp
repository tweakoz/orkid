////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>
#include <utpp/UnitTest++.h>
#include <string.h>
#include <math.h>

typedef uint32_t u32;

typedef std::atomic<int> atomic_counter;

using namespace ork;
using namespace ork::opq;

// atomic_reservoir = atomic_alloc_reservoir(USE_DEFAULT_PM,10, NULL);
// atomic_var = atomic_alloc_variable(atomic_reservoir, NULL);
// atomic_fetch_and_increment(atomic_var);
// ret_inc = atomic_load(atomic_var);

#if 1
TEST(opq_serialized_ops) {
  float fsynctime = ork::get_sync_time();

  srand(u32(fsynctime * 100.0f)); // so we dont get the same thread count seqeuence every run

  for (int i = 0; i < 16; i++) {
    const int knumthreads = 1 + rand() % 64;
    const int kopqconcurr = knumthreads;
    const int kops        = 1 << 14;

    auto gopq1                  = new OperationsQueue(knumthreads);
    gopq1->_debuginfo = get_backtrace();
    gopq1->_name = FormatString("opq_serialized_ops<%d>",i);
    auto l0grp                  = gopq1->createConcurrencyGroup("l0");
    auto l1grp                  = gopq1->createConcurrencyGroup("l1");
    auto l2grp                  = gopq1->createConcurrencyGroup("l2");
    auto l3grp                  = gopq1->createConcurrencyGroup("l3");
    l0grp->_limit_maxops_inflight = 1;
    l1grp->_limit_maxops_inflight = 1;
    l2grp->_limit_maxops_inflight = 1;
    l3grp->_limit_maxops_inflight = 1;

    std::atomic<int> ops_in_flight;

    atomic_counter l0ctr(0), l1ctr(0), l2ctr(0), l3ctr(0);

    ops_in_flight = 0;

    ork::Timer measure;
    measure.Start();

    auto l0_op = [&]() {
      int curnumops = ops_in_flight.fetch_add(1);

      assert(curnumops <= kopqconcurr);
      assert(curnumops >= 0);

      int this_l0cnt = l0ctr.fetch_add(1);

      auto l1_op = [=, &l1ctr, &l2ctr, &l3ctr]() {
        int this_l1cnt = l1ctr.fetch_add(1);
         assert(this_l1cnt==this_l0cnt);

        auto l2_op = [=, &l2ctr, &l3ctr]() {
          int this_l2cnt = l2ctr.fetch_add(1);
           assert(this_l2cnt==this_l1cnt);

          auto l3_op = [=, &l3ctr]() {
            int this_l3cnt = l3ctr.fetch_add(1);
             assert(this_l3cnt==this_l2cnt);
          };
          l3grp->enqueue(Op(l3_op)); // thr 5
        };
        l2grp->enqueue(Op(l2_op)); // thr 6, 7
      };
      l1grp->enqueue(Op(l1_op)); // thr 4

      ops_in_flight--;
    };

    for (int i = 0; i < kops; i++) {

      l0grp->enqueue(Op(l0_op, "yo"));
    }

    l0grp->drain(); // thr 2
    l1grp->drain(); // thr 2
    l2grp->drain(); // thr 2
    l3grp->drain(); // thr 2

    int this_l0cnt = l0ctr.load();
    int this_l1cnt = l1ctr.load();

    CHECK(this_l0cnt == kops);
    // CHECK(this_l1cnt==kops);

    float elapsed = measure.SecsSinceStart();

    float fopspsec = float(kops * 2) / elapsed;

    printf(
        "opq_serops<%d> test nthr<%d> elapsed<%.02f sec> nops<%d> ops/sec[agg]<%d> ops/sec[/thr]<%d> counter<%d>\n",
        i,
        knumthreads,
        elapsed,
        kops,
        int(fopspsec),
        int(fopspsec / float(knumthreads)),
        int(this_l0cnt));

    delete gopq1;
  }
}
TEST(opq_serialized_ops2) {
  float fsynctime = ork::get_sync_time();

  srand(u32(fsynctime * 100.0f)); // so we dont get the same thread count seqeuence every run

  for (int i = 0; i < 16; i++) {
    const int knumthreads = 1 + rand() % 64;
    const int kopqconcurr = knumthreads;
    const int kops        = 1 << 14;

    auto gopq1                  = new OperationsQueue(knumthreads);
    gopq1->_debuginfo = get_backtrace();
    auto l0grp                  = gopq1->createConcurrencyGroup("l0");
    auto l1grp                  = gopq1->createConcurrencyGroup("l1");
    auto l2grp                  = gopq1->createConcurrencyGroup("l2");
    auto l3grp                  = gopq1->createConcurrencyGroup("l3");
    l0grp->_limit_maxops_inflight = 0;
    l1grp->_limit_maxops_inflight = 0;
    l2grp->_limit_maxops_inflight = 0;
    l3grp->_limit_maxops_inflight = 0;

    ork::atomic<int> ops_in_flight;

    atomic_counter l0ctr(0), l1ctr(0), l2ctr(0), l3ctr(0);

    ops_in_flight = 0;

    ork::Timer measure;
    measure.Start();

    auto l0_op = [&]() {
      ops_in_flight++;

      // assert(int(ops_in_flight)<=kopqconcurr);
      // assert(int(ops_in_flight)>=0);

      int this_l0cnt = l0ctr.fetch_add(1);
      // int this_l0cnt = level0_counter++; // record l0cnt at l1 queue time

      auto l1_op = [=, &l1ctr, &l2ctr, &l3ctr]() {
        // int this_l1cnt = level1_counter++; // record l1cnt at l2 queue time
        int this_l1cnt = l1ctr.fetch_add(1);
        // assert(this_l1cnt==this_l0cnt);

        auto l2_op = [=, &l2ctr, &l3ctr]() {
          // int this_l2cnt = level2_counter++; // record l2cnt at l3 queue time
          int this_l2cnt = l2ctr.fetch_add(1);
          // assert(this_l2cnt==this_l1cnt);

          auto l3_op = [=, &l3ctr]() {
            // int this_l3cnt = level3_counter++;
            int this_l3cnt = l3ctr.fetch_add(1);
            // assert(this_l3cnt==this_l2cnt);
          };
          l3grp->enqueue(Op(l3_op));
        };
        l2grp->enqueue(Op(l2_op));
      };
      l1grp->enqueue(Op(l1_op));

      ops_in_flight--;
    };

    for (int i = 0; i < kops; i++) {

      l0grp->enqueue(Op(l0_op, "yo"));
    }

    gopq1->drain();

    int this_l0cnt = l0ctr.load();
    int this_l3cnt = l3ctr.load();

    CHECK(this_l3cnt == kops);
    // CHECK(this_l1cnt==kops);

    float elapsed = measure.SecsSinceStart();

    float fopspsec = float(kops * 2) / elapsed;

    printf(
        "opq_serops2 test nthr<%d> elapsed<%.02f sec> nops<%d> ops/sec[agg]<%d> ops/sec[/thr]<%d> counter<%d>\n",
        knumthreads,
        elapsed,
        kops,
        int(fopspsec),
        int(fopspsec / float(knumthreads)),
        int(this_l0cnt));

    delete gopq1;
  }
}
#endif
#if 1
TEST(opq_maxinflight) {
  float fsynctime = ork::get_sync_time();

  srand(u32(fsynctime * 100.0f)); // so we dont get the same thread count seqeuence every run

  for (int i = 0; i < 4; i++) {
    const int knumthreads = 1 + rand() % 16;
    const int kopqconcurr = knumthreads;
    const int kops        = knumthreads * 256;

    auto gopq1                  = new OperationsQueue(knumthreads);
    auto gopg1                  = gopq1->createConcurrencyGroup("g1");
    gopg1->_limit_maxops_inflight = kopqconcurr;

    // printf( "OpLimit::1 <%d>\n", kopqconcurr );

    std::atomic<int> ops_in_flight;
    std::atomic<int> counter;

    ops_in_flight = 0;
    counter       = 0;

    ork::Timer measure;
    measure.Start();

    for (int i = 0; i < kops; i++) {
      auto the_op = [&]() {
        ops_in_flight++;

        assert(int(ops_in_flight) <= kopqconcurr);
        assert(int(ops_in_flight) >= 0);

        int ir   = 4 + (rand() % 4);
        float fr = float(ir) * 0.0003f;

        ork::Timer throttle;
        throttle.Start();

        // printf( "BEG Op<%p> wait<%dms> OIF<%d>\n", this, ir, int(ops_in_flight) );

        int i = 0;

        while (throttle.SecsSinceStart() < fr) {
          usleep(100);
        }

        ops_in_flight--;

        counter++;
      };

      gopg1->enqueue(Op(the_op, "yo"));

      int j = rand() % 1000;
      if (j < 10)
        gopq1->drain();
    }

    // gopg1->drain();
    gopq1->drain();

    CHECK(counter == kops);

    float elapsed = measure.SecsSinceStart();

    float fopspsec = float(kops) / elapsed;

    printf(
        "opqmaxinflight test nthr<%d> elapsed<%.02f sec> nops<%d> ops/sec[agg]<%d> ops/sec[/thr]<%d> counter<%d>\n",
        knumthreads,
        elapsed,
        kops,
        int(fopspsec),
        int(fopspsec / float(knumthreads)),
        int(counter));

    delete gopq1;
  }
}
#endif
#if 1

TEST(opq_ballsout) {
  float fsynctime = ork::get_sync_time();

  srand(u32(fsynctime * 100.0f)); // so we dont get the same thread count seqeuence every run

  for (int i = 0; i < 16; i++) {
    const int knumthreads = 1 + rand() % 64;
    const int kopqconcurr = knumthreads;
    const int kops        = knumthreads * 256;

    auto gopq1                  = new OperationsQueue(knumthreads);
    auto gopg1                  = gopq1->createConcurrencyGroup("g1");
    gopg1->_limit_maxops_inflight = 0;

    ork::atomic<int> ops_in_flight;
    ork::atomic<int> counter;

    ops_in_flight = 0;
    counter       = 0;

    ork::Timer measure;
    measure.Start();

    for (int i = 0; i < kops; i++) {
      gopg1->enqueue(Op(
          [&]() {
            ops_in_flight++;
            assert(int(ops_in_flight) <= kopqconcurr);
            assert(int(ops_in_flight) >= 0);
            ops_in_flight--;
            counter++;
          },
          "yo"));
    }

    gopg1->drain();
    gopq1->drain();

    CHECK(counter == kops);

    float elapsed = measure.SecsSinceStart();

    float fopspsec = float(kops) / elapsed;

    printf(
        "opq_ballsout test nthr<%d> elapsed<%.02f sec> nops<%d> ops/sec[agg]<%d> ops/sec[/thr]<%d> counter<%d>\n",
        knumthreads,
        elapsed,
        kops,
        int(fopspsec),
        int(fopspsec / float(knumthreads)),
        int(counter));

    delete gopq1;
  }
}

TEST(opq_real_load) {
  float fsynctime = ork::get_sync_time();

  srand(u32(fsynctime * 100.0f)); // so we dont get the same thread count seqeuence every run

  float favgopspsec = 0.0f;

  const int kdim = 512;

  for (int i = 0; i < 16; i++) {
    const int knumthreads = 8 + rand() % 64;
    const int kopqconcurr = knumthreads;
    const int kops        = 256;

    auto gopq1                  = new OperationsQueue(knumthreads);
    auto gopg1                  = gopq1->createConcurrencyGroup("g1");
    gopg1->_limit_maxops_inflight = 0;

    ork::atomic<int> ops_in_flight;
    ork::atomic<int> counter;

    ops_in_flight = 0;
    counter       = 0;

    ork::Timer measure;
    measure.Start();

    atomic_counter no_opt;
    no_opt.store(0);

    for (int i = 0; i < kops; i++) {
      gopg1->enqueue(Op(
          [&]() {
            int nops = 0;
            ops_in_flight++;

            float* pbuf = new float[kdim * kdim];
            float fidim = 1.0f / float(kdim);

            for (int y = 0; y < kdim; y++) {
              float fy = float(y) * fidim;
              for (int x = 0; x < kdim; x++) {
                float fx  = float(x) * fidim;
                int idx   = (y * kdim) + x;
                pbuf[idx] = sinf(fy * PI2) * cosf(fx * PI2) + tanf(fy * fx * PI2);
                nops += int(pbuf[idx]);
              }
            }
            delete[] pbuf;

            ops_in_flight--;
            counter++;
            no_opt.fetch_add(nops);
          },
          "yo"));
    }

    gopg1->drain();
    gopq1->drain();

    CHECK(counter == kops);

    float elapsed = measure.SecsSinceStart();

    float fopspsec = float(kops) / elapsed;
    favgopspsec += fopspsec;

    printf(
        "opq_real_load nthr<%d> elapsed<%.04f sec> nops<%d> ops/sec[agg]<%d> ops/sec[/thr]<%d> counter<%d> no_opt<%d>\n",
        knumthreads,
        elapsed,
        kops,
        int(fopspsec),
        int(fopspsec / float(knumthreads)),
        int(counter),
        int(no_opt.load()));

    delete gopq1;
  }
  float agg   = favgopspsec / float(16.0f);
  float fMPPS = float(kdim * kdim) * agg / 1000000.0f;
  printf("opq_real_load avg_ops/sec(agg) %f MPPS<%f>\n", agg, fMPPS);
}

#endif
