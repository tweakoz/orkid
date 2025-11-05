////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>
#include <cmath>
#include <atomic>

using namespace ork;
using namespace ork::opq;

void runOpqTests(void) {
    auto logchan = logger()->configureChannel("OPQ", fvec3(0.3f, 0.8f, 0.9f), true);

    logchan->log("========================================");
    logchan->log("Starting OPQ Tests");
    logchan->log("Operation Queue Parallelism");
    logchan->log("========================================");

    // Test 1: Ballsout - Maximum throughput test
    logchan->log("");
    logchan->log("--- Test 1: Ballsout (Max Throughput) ---");

    {
        float fsynctime = Timer::get_sync_time();
        srand(uint32_t(fsynctime * 100.0f));

        float total_ops_per_sec = 0.0f;
        const int num_iterations = 8;

        for (int i = 0; i < num_iterations; i++) {
            const int knumthreads = 4 + rand() % 8;  // 4-11 threads
            const int kops = knumthreads * 4096;     // Scale ops with threads (8x)

            auto opq = new OperationsQueue(knumthreads);
            auto grp = opq->createConcurrencyGroup("ballsout");
            grp->_limit_maxops_inflight = 0;  // Unlimited

            std::atomic<int> ops_in_flight(0);
            std::atomic<int> counter(0);

            Timer measure;
            measure.Start();

            // Enqueue all operations
            for (int j = 0; j < kops; j++) {
                grp->enqueue(Op([&]() {
                    ops_in_flight++;
                    // Minimal work - just increment counters
                    ops_in_flight--;
                    counter++;
                }, "ballsout_op"));
            }

            // Drain and wait for completion
            grp->drain();
            opq->drain();

            float elapsed = measure.SecsSinceStart();
            float ops_per_sec = float(kops) / elapsed;
            total_ops_per_sec += ops_per_sec;

            logchan->log("  Iter %d: threads=%d ops=%d elapsed=%.4fs ops/sec=%d counter=%d",
                        i, knumthreads, kops, elapsed,
                        int(ops_per_sec), int(counter));

            delete opq;
        }

        float avg_ops_per_sec = total_ops_per_sec / float(num_iterations);
        logchan->log("✓ Ballsout average: %.0f ops/sec", avg_ops_per_sec);
    }

    // Test 2: Real Load - Significant computational work
    logchan->log("");
    logchan->log("--- Test 2: Real Load (Computational Work) ---");

    {
        float fsynctime = Timer::get_sync_time();
        srand(uint32_t(fsynctime * 100.0f));

        float total_ops_per_sec = 0.0f;
        const int num_iterations = 8;
        const int kdim = 512;  // 512x512 float array

        for (int i = 0; i < num_iterations; i++) {
            const int knumthreads = 4 + rand() % 8;  // 4-11 threads
            const int kops = 1024;  // Fixed ops count (8x)

            auto opq = new OperationsQueue(knumthreads);
            auto grp = opq->createConcurrencyGroup("real_load");
            grp->_limit_maxops_inflight = 0;  // Unlimited

            std::atomic<int> ops_in_flight(0);
            std::atomic<int> counter(0);
            std::atomic<int> no_opt(0);  // Prevent optimization

            Timer measure;
            measure.Start();

            // Enqueue computationally intensive operations
            for (int j = 0; j < kops; j++) {
                grp->enqueue(Op([&]() {
                    ops_in_flight++;

                    // Allocate and compute on 512x512 float array
                    float* pbuf = new float[kdim * kdim];
                    float fidim = 1.0f / float(kdim);
                    int nops = 0;

                    // Embarrassingly parallel computation
                    for (int y = 0; y < kdim; y++) {
                        float fy = float(y) * fidim;
                        for (int x = 0; x < kdim; x++) {
                            float fx = float(x) * fidim;
                            int idx = (y * kdim) + x;

                            // Trig operations on each element
                            pbuf[idx] = sinf(fy * PI2) * cosf(fx * PI2) + tanf(fy * fx * PI2);
                            nops += int(pbuf[idx]);
                        }
                    }

                    delete[] pbuf;

                    ops_in_flight--;
                    counter++;
                    no_opt.fetch_add(nops);
                }, "real_load_op"));
            }

            // Drain and wait for completion
            grp->drain();
            opq->drain();

            float elapsed = measure.SecsSinceStart();
            float ops_per_sec = float(kops) / elapsed;
            total_ops_per_sec += ops_per_sec;

            // Calculate MPPS (Million Pixels Per Second)
            float mpps = float(kdim * kdim) * ops_per_sec / 1000000.0f;

            logchan->log("  Iter %d: threads=%d ops=%d elapsed=%.4fs ops/sec=%d MPPS=%.2f no_opt=%d",
                        i, knumthreads, kops, elapsed,
                        int(ops_per_sec), mpps, int(no_opt.load()));

            delete opq;
        }

        float avg_ops_per_sec = total_ops_per_sec / float(num_iterations);
        float avg_mpps = float(kdim * kdim) * avg_ops_per_sec / 1000000.0f;
        logchan->log("✓ Real Load average: %.0f ops/sec, %.2f MPPS",
                    avg_ops_per_sec, avg_mpps);
    }

    // Test 3: Max Inflight Limit
    logchan->log("");
    logchan->log("--- Test 3: Max Inflight Limit ---");

    {
        const int knumthreads = 8;
        const int kopqconcurr = knumthreads;
        const int kops = knumthreads * 1024;  // 8x

        auto opq = new OperationsQueue(knumthreads);
        auto grp = opq->createConcurrencyGroup("limited");
        grp->_limit_maxops_inflight = kopqconcurr;

        std::atomic<int> ops_in_flight(0);
        std::atomic<int> counter(0);
        std::atomic<int> max_observed(0);

        Timer measure;
        measure.Start();

        for (int i = 0; i < kops; i++) {
            grp->enqueue(Op([&]() {
                int current = ops_in_flight.fetch_add(1) + 1;

                // Track maximum concurrent operations
                int prev_max = max_observed.load();
                while (current > prev_max &&
                       !max_observed.compare_exchange_weak(prev_max, current)) {
                    prev_max = max_observed.load();
                }

                // Verify we don't exceed limit
                if (current > kopqconcurr) {
                    logchan->log("✗ VIOLATION: %d ops in flight (limit: %d)",
                                current, kopqconcurr);
                }

                // Small amount of work
                int sum = 0;
                for (int j = 0; j < 1000; j++) {
                    sum += j;
                }
                counter.fetch_add(sum % 2);  // Prevent optimization

                ops_in_flight--;
            }, "limited_op"));
        }

        grp->drain();
        opq->drain();

        float elapsed = measure.SecsSinceStart();

        logchan->log("  Threads: %d", knumthreads);
        logchan->log("  Operations: %d", kops);
        logchan->log("  Limit: %d", kopqconcurr);
        logchan->log("  Max observed: %d", int(max_observed.load()));
        logchan->log("  Elapsed: %.4fs", elapsed);

        if (max_observed.load() <= kopqconcurr) {
            logchan->log("✓ Max inflight limit respected");
        } else {
            logchan->log("✗ Max inflight limit VIOLATED");
        }

        delete opq;
    }

    // Test 4: Nested Operations (Pipeline)
    logchan->log("");
    logchan->log("--- Test 4: Nested Operations (Pipeline) ---");

    {
        const int knumthreads = 8;
        const int kops = 2048;  // 8x

        auto opq = new OperationsQueue(knumthreads);
        auto stage1 = opq->createConcurrencyGroup("stage1");
        auto stage2 = opq->createConcurrencyGroup("stage2");
        auto stage3 = opq->createConcurrencyGroup("stage3");

        stage1->_limit_maxops_inflight = 0;
        stage2->_limit_maxops_inflight = 0;
        stage3->_limit_maxops_inflight = 0;

        std::atomic<int> stage1_counter(0);
        std::atomic<int> stage2_counter(0);
        std::atomic<int> stage3_counter(0);

        Timer measure;
        measure.Start();

        // Enqueue pipeline operations
        for (int i = 0; i < kops; i++) {
            stage1->enqueue(Op([&, i]() {
                stage1_counter++;

                // Stage 2: Process data
                stage2->enqueue(Op([&, i]() {
                    stage2_counter++;

                    // Stage 3: Finalize
                    stage3->enqueue(Op([&, i]() {
                        stage3_counter++;
                    }, "stage3"));
                }, "stage2"));
            }, "stage1"));
        }

        // Drain all stages
        stage1->drain();
        stage2->drain();
        stage3->drain();
        opq->drain();

        float elapsed = measure.SecsSinceStart();

        logchan->log("  Operations: %d", kops);
        logchan->log("  Stage 1 completed: %d", int(stage1_counter.load()));
        logchan->log("  Stage 2 completed: %d", int(stage2_counter.load()));
        logchan->log("  Stage 3 completed: %d", int(stage3_counter.load()));
        logchan->log("  Elapsed: %.4fs", elapsed);

        if (stage3_counter.load() == kops) {
            logchan->log("✓ Pipeline completed all operations");
        } else {
            logchan->log("✗ Pipeline incomplete");
        }

        delete opq;
    }

    logchan->log("");
    logchan->log("========================================");
    logchan->log("OPQ Tests Complete");
    logchan->log("========================================");
}
