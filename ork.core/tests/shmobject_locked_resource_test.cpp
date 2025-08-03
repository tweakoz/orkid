#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/util/shmobject.h>
#include <ork/util/shmobject_locked_resource.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/filedev.h>
#include <ork/file/path.h>
#include <utpp/UnitTest++.h>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <atomic>

using namespace ork;

////////////////////////////////////////////////////////////////
// Test data structure - must be POD
////////////////////////////////////////////////////////////////

struct StressTestCounters {
  uint64_t counter1 = 0;
  uint64_t counter2 = 0;
  uint64_t counter3 = 0;
  uint64_t counter4 = 0;
  uint64_t sum_check = 0;  // Should always equal counter1+counter2+counter3+counter4
  uint32_t concurrent_access_detector = 0;  // Should always be 0 or 1
  uint32_t error_count = 0;
};

using locked_counters_t = ShmLockedResource<StressTestCounters>;
using shared_counters_mem_t = ShmObject<locked_counters_t>;

////////////////////////////////////////////////////////////////

TEST(ShmLockedResource_BasicOps) {
  printf("Testing basic operations...\n");
  
  // Realize shared memory with locked resource
  auto shm = shared_counters_mem_t::realize("test_basic");
  
  // Get resource pointer (already initialized by ShmObject)
  auto* resource = shm->image();
  
  // Test atomic write
  StressTestCounters initial{};
  initial.counter1 = 100;
  initial.counter2 = 200;
  resource->atomicWrite(initial);
  
  // Test atomic copy
  auto copy = resource->atomicCopy();
  CHECK_EQUAL(100, copy.counter1);
  CHECK_EQUAL(200, copy.counter2);
  
  // Test atomic operation
  resource->atomicOp([](StressTestCounters& c) {
    c.counter1 += 50;
    c.counter2 *= 2;
  });
  
  copy = resource->atomicCopy();
  CHECK_EQUAL(150, copy.counter1);
  CHECK_EQUAL(400, copy.counter2);
  
  // Test atomic exchange
  StressTestCounters new_val{};
  new_val.counter3 = 999;
  auto old = resource->atomicExchange(new_val);
  CHECK_EQUAL(150, old.counter1);
  CHECK_EQUAL(400, old.counter2);
  
  copy = resource->atomicCopy();
  CHECK_EQUAL(999, copy.counter3);
  CHECK_EQUAL(0, copy.counter1);
}

////////////////////////////////////////////////////////////////

TEST(ShmLockedResource_ConcurrentStress) {
  printf("Testing concurrent stress with multiple threads...\n");
  
  // Realize shared memory with locked resource
  auto shm = shared_counters_mem_t::realize("test_stress");
  auto* resource = shm->image();
    
  const int NUM_THREADS = 16;
  const int OPS_PER_THREAD = 10000;
  std::vector<std::thread> threads;
  std::atomic<int> start_signal{0};
  std::atomic<int> threads_ready{0};
  
  Timer timer;
  timer.Start();
  
  // Spawn worker threads
  for (int tid = 0; tid < NUM_THREADS; ++tid) {
    threads.emplace_back([&, tid]() {
      SetCurrentThreadName(FormatString("worker_%d", tid).c_str());
      
      std::mt19937 rng(tid);
      std::uniform_int_distribution<int> op_dist(0, 5);
      std::uniform_int_distribution<int> counter_dist(0, 3);
      
      // Signal ready
      threads_ready.fetch_add(1);
      
      // Wait for start signal
      while (start_signal.load() == 0) {
        std::this_thread::yield();
      }
      
      // Hammer the resource
      for (int op = 0; op < OPS_PER_THREAD; ++op) {
        int op_type = op_dist(rng);
        
        switch (op_type) {
          case 0: {
            // Increment random counter with consistency check
            resource->atomicOp([&](StressTestCounters& c) {
              // Check concurrent access detector
              if (c.concurrent_access_detector != 0) {
                c.error_count++;
              }
              c.concurrent_access_detector = 1;
              
              // Increment a counter
              int which = counter_dist(rng);
              switch (which) {
                case 0: c.counter1++; break;
                case 1: c.counter2++; break;
                case 2: c.counter3++; break;
                case 3: c.counter4++; break;
              }
              
              // Update sum check
              c.sum_check = c.counter1 + c.counter2 + c.counter3 + c.counter4;
              
              // Simulate some work
              for (volatile int i = 0; i < 10; ++i) {}
              
              c.concurrent_access_detector = 0;
            });
            break;
          }
          
          case 1: {
            // Read and verify consistency
            auto snapshot = resource->atomicCopy();
            uint64_t expected_sum = snapshot.counter1 + snapshot.counter2 + 
                                  snapshot.counter3 + snapshot.counter4;
            if (snapshot.sum_check != expected_sum) {
              printf("ERROR: Consistency check failed! sum_check=%lu expected=%lu\n",
                     snapshot.sum_check, expected_sum);
            }
            break;
          }
          
          case 2: {
            // Manual lock/unlock with multiple operations
            {
              auto& c = resource->lockForWrite();
              if (c.concurrent_access_detector != 0) {
                c.error_count++;
              }
              c.concurrent_access_detector = 1;
              
              // Do multiple updates
              c.counter1 += 2;
              c.counter2 += 3;
              c.counter3 += 4;
              c.counter4 += 5;
              c.sum_check = c.counter1 + c.counter2 + c.counter3 + c.counter4;
              
              // Simulate work
              for (volatile int i = 0; i < 20; ++i) {}
              
              c.concurrent_access_detector = 0;
              resource->unlock();
            }
            break;
          }
          
          case 3: {
            // Try lock with timeout simulation
            if (resource->tryLock()) {
              auto& c = resource->_unprotected_ref();
              if (c.concurrent_access_detector != 0) {
                c.error_count++;
              }
              c.concurrent_access_detector = 1;
              
              c.counter1++;
              c.sum_check = c.counter1 + c.counter2 + c.counter3 + c.counter4;
              
              c.concurrent_access_detector = 0;
              resource->unlock();
            }
            break;
          }
          
          case 4: {
            // Atomic exchange test
            StressTestCounters local = resource->atomicCopy();
            local.counter1++;
            local.counter2++;
            local.sum_check = local.counter1 + local.counter2 + local.counter3 + local.counter4;
            resource->atomicExchange(local);
            break;
          }
          
          case 5: {
            // Nested locking test (recursive)
            resource->atomicOp([&](StressTestCounters& c) {
              c.counter1++;
              
              // Nested atomic op (tests recursive locking)
              resource->atomicOp([&](StressTestCounters& c2) {
                c2.counter2++;
                c2.sum_check = c2.counter1 + c2.counter2 + c2.counter3 + c2.counter4;
              });
            });
            break;
          }
        }
        
        // Occasional yield to increase contention variety
        if (op % 100 == 0) {
          std::this_thread::yield();
        }
      }
    });
  }
  
  // Wait for all threads to be ready
  while (threads_ready.load() < NUM_THREADS) {
    usleep(1000);
  }
  
  printf("All threads ready, starting stress test...\n");
  
  // Start all threads simultaneously
  start_signal.store(1);
  
  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }
  
  double elapsed = timer.SecsSinceStart();
  
  // Final verification
  auto final_state = resource->atomicCopy();
  uint64_t expected_sum = final_state.counter1 + final_state.counter2 + 
                         final_state.counter3 + final_state.counter4;
  
  printf("\n=== Stress Test Results ===\n");
  printf("Time: %.3f seconds\n", elapsed);
  printf("Total operations: %d\n", NUM_THREADS * OPS_PER_THREAD);
  printf("Ops/sec: %.0f\n", (NUM_THREADS * OPS_PER_THREAD) / elapsed);
  printf("Counter1: %lu\n", final_state.counter1);
  printf("Counter2: %lu\n", final_state.counter2);
  printf("Counter3: %lu\n", final_state.counter3);
  printf("Counter4: %lu\n", final_state.counter4);
  printf("Sum check: %lu (expected: %lu)\n", final_state.sum_check, expected_sum);
  printf("Concurrent access errors: %u\n", final_state.error_count);
  printf("Concurrent detector state: %u (should be 0)\n", final_state.concurrent_access_detector);
  
  CHECK_EQUAL(expected_sum, final_state.sum_check);
  CHECK_EQUAL(0, final_state.error_count);
  CHECK_EQUAL(0, final_state.concurrent_access_detector);
}

////////////////////////////////////////////////////////////////

TEST(ShmLockedResource_MultiProcess) {
  printf("Testing multi-process access...\n");
  const char* shm_name = "test_multiproc";

  // Parent creates the shared memory
  printf("getting shm: %s\n", shm_name);
  auto shm = shared_counters_mem_t::realize(shm_name);
  auto* par_resource = shm->image();
  printf("got shm: %s\n",shm_name);
    
  const int NUM_PROCESSES = 4;
  const int OPS_PER_PROCESS = 1000;
  
  Timer timer;
  timer.Start();
  
  // Fork child processes
  std::vector<pid_t> children;
  for (int i = 0; i < NUM_PROCESSES; ++i) {
    pid_t pid = fork();
    if (pid == 0) {
      // Child process - use _exit() to avoid cleanup issues
      auto child_shm = shared_counters_mem_t::realize(shm_name);
      auto* child_resource = child_shm->image();
      
      // Each process increments different counters
      for (int op = 0; op < OPS_PER_PROCESS; ++op) {
        child_resource->atomicOp([i](StressTestCounters& c) {
          // Check concurrent access
          if (c.concurrent_access_detector != 0) {
            c.error_count++;
          }
          c.concurrent_access_detector = 1;
          
          // Each process works on its own counter
          switch (i % 4) {
            case 0: c.counter1++; break;
            case 1: c.counter2++; break;
            case 2: c.counter3++; break;
            case 3: c.counter4++; break;
          }
          
          c.sum_check = c.counter1 + c.counter2 + c.counter3 + c.counter4;
          
          // Simulate work
          for (volatile int j = 0; j < 100; ++j) {}
          
          c.concurrent_access_detector = 0;
        });
        
        // Occasional read
        if (op % 100 == 0) {
          auto snapshot = child_resource->atomicCopy();
          uint64_t sum = snapshot.counter1 + snapshot.counter2 + 
                        snapshot.counter3 + snapshot.counter4;
          if (snapshot.sum_check != sum) {
            printf("Child %d: Consistency error!\n", i);
          }
        }
      }
      
      _exit(0);  // Child exits without cleanup to avoid thread issues
    } else if (pid > 0) {
      children.push_back(pid);
    } else {
      perror("fork failed");
    }
  }
  
  // Parent waits for all children
  for (pid_t child : children) {
    int status;
    waitpid(child, &status, 0);
  }
  
  double elapsed = timer.SecsSinceStart();
  
  // Verify final state
  auto final_state = par_resource->atomicCopy();
  uint64_t expected_sum = final_state.counter1 + final_state.counter2 + 
                         final_state.counter3 + final_state.counter4;
  uint64_t expected_total = NUM_PROCESSES * OPS_PER_PROCESS;
  
  printf("\n=== Multi-Process Test Results ===\n");
  printf("Time: %.3f seconds\n", elapsed);
  printf("Counter1: %lu\n", final_state.counter1);
  printf("Counter2: %lu\n", final_state.counter2);
  printf("Counter3: %lu\n", final_state.counter3);
  printf("Counter4: %lu\n", final_state.counter4);
  printf("Total increments: %lu (expected: %lu)\n", expected_sum, expected_total);
  printf("Sum check: %lu\n", final_state.sum_check);
  printf("Concurrent access errors: %u\n", final_state.error_count);
  
  CHECK_EQUAL(expected_total, expected_sum);
  CHECK_EQUAL(expected_sum, final_state.sum_check);
  CHECK_EQUAL(0, final_state.error_count);
  CHECK_EQUAL(0, final_state.concurrent_access_detector);
}

////////////////////////////////////////////////////////////////

TEST(ShmLockedResource_DeathTest) {
  printf("Testing mutex ownership violations...\n");
  
  auto shm = shared_counters_mem_t::realize("test_death");
  auto* resource = shm->image();
  
  // This test would normally abort() on unlock without lock
  // We can't actually test it without crashing, so we just verify
  // the mutex properly tracks ownership
  
  // Lock in one thread
  std::thread t1([&]() {
    resource->lockForWrite();
    // Don't unlock - let thread die with lock held
  });
  t1.join();
  
  // Try to lock from main thread - should spin forever or timeout
  // We'll use tryLock instead
  bool got_lock = resource->tryLock();
  CHECK_EQUAL(false, got_lock);  // Should fail since other thread died with lock
  
  printf("Death test completed (ownership tracking verified)\n");
}