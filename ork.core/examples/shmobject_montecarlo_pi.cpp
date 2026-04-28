#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/util/shmobject.h>
#include <ork/util/shmobject_locked_resource.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/filedev.h>
#include <ork/file/path.h>
#include <iostream>
#include <random>
#include <atomic>
#include <sys/wait.h>

using namespace ork;

namespace ork {
  void initModule(appinitdata_ptr_t appinit);
  void exitModule(appinitdata_ptr_t appinit);
}

////////////////////////////////////////////////////////////////
// Shared state for Monte Carlo Pi calculation
////////////////////////////////////////////////////////////////

struct MonteCarloPiState {
  uint64_t total_points = 0;      // Total random points generated
  uint64_t inside_circle = 0;     // Points that fell inside unit circle
  uint64_t process_contributions[16] = {0}; // Track each process's contribution
  double current_pi_estimate = 0.0;
  uint64_t checksum = 0;          // Should always equal total_points
};

using locked_pi_state_t = ShmLockedResource<MonteCarloPiState>;
using shared_pi_mem_t = ShmObject<locked_pi_state_t>;

////////////////////////////////////////////////////////////////

void monte_carlo_worker(int process_id, int num_samples) {
  // Realize shared memory (size defaults to sizeof(T))
  auto shm = shared_pi_mem_t::realize("montecarlo_pi");
  auto* state = shm->image();
  
  // Initialize random number generator with unique seed per process
  std::mt19937_64 rng(process_id * 1000000 + getpid());
  std::uniform_real_distribution<double> dist(-1.0, 1.0);
  
  uint64_t local_inside = 0;
  uint64_t local_total = 0;
  
  // Do computation in batches to reduce contention
  const int BATCH_SIZE = 1048576;
  int num_batches = num_samples / BATCH_SIZE;
  
  for (int batch = 0; batch < num_batches; ++batch) {
    // Compute batch locally
    for (int i = 0; i < BATCH_SIZE; ++i) {
      double x = dist(rng);
      double y = dist(rng);
      
      // Check if point is inside unit circle
      if (x*x + y*y <= 1.0) {
        local_inside++;
      }
      local_total++;
    }
    
    // Update shared state atomically every batch
    if (batch % 10 == 0) {  // Update every 10 batches to reduce contention
      state->atomicOp([&](MonteCarloPiState& s) {
        s.total_points += local_total;
        s.inside_circle += local_inside;
        s.process_contributions[process_id] += local_total;
        s.checksum += local_total;
        
        // Update Pi estimate
        if (s.total_points > 0) {
          s.current_pi_estimate = 4.0 * double(s.inside_circle) / double(s.total_points);
        }
      });
      
      // Reset local counters
      local_total = 0;
      local_inside = 0;
    }
  }
  
  // Final update with remaining samples
  if (local_total > 0) {
    state->atomicOp([&](MonteCarloPiState& s) {
      s.total_points += local_total;
      s.inside_circle += local_inside;
      s.process_contributions[process_id] += local_total;
      s.checksum += local_total;
      
      if (s.total_points > 0) {
        s.current_pi_estimate = 4.0 * double(s.inside_circle) / double(s.total_points);
      }
    });
  }
}

////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  SetCurrentThreadName("main");
  genviron.init_from_global_env();
  
  // Initialize application
  auto appinit = std::make_shared<AppInitData>(argc, argv);
  appinit->_update_rendersync = false;
  appinit->_enable_graphics = false;
  appinit->_enable_audio = false;
  
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
  initModule(appinit);
  appinit->executePostInitOps();
  
  printf("=== Multi-Process Monte Carlo Pi Calculation ===\n");
  printf("Using shared memory for perfect synchronization\n\n");
  
  // Configuration
  const int NUM_PROCESSES = 16;
  const int SAMPLES_PER_PROCESS = 1<<30;  // 10 million samples per process
  const uint64_t TOTAL_SAMPLES = uint64_t(NUM_PROCESSES) * uint64_t(SAMPLES_PER_PROCESS);
  
  printf("Configuration:\n");
  printf("  Processes: %d\n", NUM_PROCESSES);
  printf("  Samples per process: %d\n", SAMPLES_PER_PROCESS);
  printf("  Total samples: %llu\n", (ull)TOTAL_SAMPLES);
  printf("\n");
  
  // Realize shared memory and initialize (size defaults to sizeof(T))
  auto shm = shared_pi_mem_t::realize("montecarlo_pi");
  auto* state = shm->image();
  
  Timer timer;
  timer.Start();
  
  // Fork child processes
  std::vector<pid_t> children;
  for (int i = 0; i < NUM_PROCESSES; ++i) {
    pid_t pid = fork();
    if (pid == 0) {
      // Child process
      printf("Process %d (PID %d) starting computation...\n", i, getpid());
      monte_carlo_worker(i, SAMPLES_PER_PROCESS);
      printf("Process %d (PID %d) completed.\n", i, getpid());
      _exit(0);  // Clean exit without cleanup
    } else if (pid > 0) {
      children.push_back(pid);
    } else {
      perror("fork failed");
      exit(1);
    }
  }
  
  // Parent monitors progress
  printf("\nProgress:\n");
  uint64_t last_total = 0;
  while (true) {
    usleep(500000);  // Check every 0.5 seconds
    
    auto snapshot = state->atomicCopy();
    
    // Print progress
    double progress = 100.0 * snapshot.total_points / TOTAL_SAMPLES;
    uint64_t rate = (snapshot.total_points - last_total) * 2;  // samples per second
    
    printf("\r  %.1f%% complete | %llu samples | π ≈ %.6f | Rate: %llu samples/sec     ",
           progress, (ull)snapshot.total_points, snapshot.current_pi_estimate, (ull)rate);
    fflush(stdout);
    
    last_total = snapshot.total_points;
    
    // Check if all processes are done
    if (snapshot.total_points >= TOTAL_SAMPLES) {
      break;
    }
    
    // Also check if all children have exited
    bool all_done = true;
    for (pid_t child : children) {
      int status;
      pid_t result = waitpid(child, &status, WNOHANG);
      if (result == 0) {
        all_done = false;
      }
    }
    if (all_done && snapshot.total_points > 0) {
      break;
    }
  }
  
  printf("\n\n");
  
  // Wait for all children to complete
  for (pid_t child : children) {
    int status;
    waitpid(child, &status, 0);
  }
  
  double elapsed = timer.SecsSinceStart();
  
  // Final results
  auto final_state = state->atomicCopy();
  double final_pi = 4.0 * double(final_state.inside_circle) / double(final_state.total_points);
  double error = std::abs(final_pi - M_PI);
  double error_percent = 100.0 * error / M_PI;
  
  printf("=== Final Results ===\n");
  printf("Time: %.3f seconds\n", elapsed);
  printf("Total samples: %llu\n", (ull)final_state.total_points);
  printf("Inside circle: %llu\n", (ull)final_state.inside_circle);
  printf("Calculated Pi: %.10f\n", final_pi);
  printf("Actual Pi:     %.10f\n", M_PI);
  printf("Error: %.10f (%.4f%%)\n", error, error_percent);
  printf("Throughput: %.2f million samples/second\n", 
         (final_state.total_points / elapsed) / 1000000.0);
  
  // Verify synchronization correctness
  printf("\n=== Synchronization Verification ===\n");
  
  // Check 1: Total points equals sum of contributions
  uint64_t sum_contributions = 0;
  for (int i = 0; i < NUM_PROCESSES; ++i) {
    if (final_state.process_contributions[i] > 0) {
      printf("Process %d contributed: %llu samples\n",
             i, (ull)final_state.process_contributions[i]);
      sum_contributions += final_state.process_contributions[i];
    }
  }
  
  printf("\nTotal from contributions: %llu\n", (ull)sum_contributions);
  printf("Total points recorded: %llu\n", (ull)final_state.total_points);
  printf("Checksum: %llu\n", (ull)final_state.checksum);
  
  bool sync_correct = (sum_contributions == final_state.total_points) &&
                      (final_state.checksum == final_state.total_points);
  
  if (sync_correct) {
    printf("✅ SYNCHRONIZATION CORRECT: All counts match perfectly!\n");
  } else {
    printf("❌ SYNCHRONIZATION ERROR: Counts don't match!\n");
  }
  
  // Check 2: Verify Pi estimate is reasonable
  if (error_percent < 1.0) {
    printf("✅ PI ESTIMATE ACCURATE: Error < 1%%\n");
  } else {
    printf("⚠️  PI ESTIMATE: Error = %.2f%% (expected < 1%% with %llu samples)\n",
           error_percent, (ull)final_state.total_points);
  }
  
  // Performance comparison
  printf("\n=== Performance Analysis ===\n");
  double single_process_estimate = (double(TOTAL_SAMPLES) / 1000000.0) / 
                                   ((final_state.total_points / elapsed) / 1000000.0);
  printf("Estimated single-process time: %.2f seconds\n", single_process_estimate);
  printf("Actual multi-process time: %.2f seconds\n", elapsed);
  printf("Speedup: %.2fx\n", single_process_estimate / elapsed);
  printf("Efficiency: %.1f%%\n", 100.0 * (single_process_estimate / elapsed) / NUM_PROCESSES);
  
  exitModule(appinit);
  return sync_correct ? 0 : 1;
}