#include <ork/application/application.h>
#include <ork/util/shmobject.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/filedev.h>
#include <ork/file/path.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <random>
#include <vector>

using namespace ork;

namespace ork {
  void initModule(appinitdata_ptr_t appinit);
  void exitModule(appinitdata_ptr_t appinit);
}

// Shared memory test data structure - must be POD-like for shared memory
struct SharedTestData {
    std::atomic<uint64_t> process_counter;        // How many processes have joined
    std::atomic<uint64_t> computation_counter;    // Total computations done
    std::atomic<uint64_t> checksum;              // Running checksum of all work
    std::atomic<bool> shutdown_requested;         // Coordinator requests shutdown
    std::atomic<uint64_t> shutdown_counter;       // How many have acknowledged shutdown
    
    // Work arrays for computation
    uint64_t work_array[1000];                    // Array for shared computation
    std::atomic<uint64_t> work_index;            // Next work item to process
    
    // Initialization function called by winner only
    void initializeShmImage() {
        process_counter.store(0);
        computation_counter.store(0);
        checksum.store(0);
        shutdown_requested.store(false);
        shutdown_counter.store(0);
        work_index.store(0);
        
        // Initialize work array with deterministic values
        for (int i = 0; i < 1000; ++i) {
            work_array[i] = (uint64_t(i) * 1103515245ULL + 12345ULL) ^ (uint64_t(i) << 32);
        }
    }
    
    void uninitializeShmImage() {
        // No special cleanup needed for atomics
    }
};

// Configuration
struct TestConfig {
    int num_processes = 8;           // Number of child processes to fork
    int work_duration_ms = 2000;     // How long each process works (ms)
    int max_startup_delay_ms = 1000; // Maximum random startup delay (ms)
    int coordinator_duration_ms = 5000; // How long coordinator runs before shutdown
    const char* shm_name = "stress_test_shm";
};

void print_usage(const char* prog_name) {
    printf("ShmObject Stress Test\n");
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -p <num>    Number of processes (default: 8)\n");
    printf("  -w <ms>     Work duration per process (default: 2000ms)\n");
    printf("  -d <ms>     Max startup delay (default: 1000ms)\n");
    printf("  -c <ms>     Coordinator duration (default: 5000ms)\n");
    printf("  -h          Show this help\n");
    printf("\nExample: %s -p 20 -w 3000 -d 2000\n", prog_name);
}

TestConfig parse_args(int argc, char** argv) {
    TestConfig config;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config.num_processes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config.work_duration_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config.max_startup_delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config.coordinator_duration_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
    }
    
    // Validate config
    if (config.num_processes < 1 || config.num_processes > 100) {
        printf("Error: Number of processes must be 1-100\n");
        exit(1);
    }
    if (config.work_duration_ms < 100) {
        printf("Error: Work duration must be at least 100ms\n");
        exit(1);
    }
    
    return config;
}

void worker_process(int worker_id, const TestConfig& config) {
    printf("[WORKER %d] Starting (PID: %d)\n", worker_id, getpid());
    
    // Random startup delay to stress timing
    std::random_device rd;
    std::mt19937 gen(rd() ^ getpid() ^ worker_id);
    std::uniform_int_distribution<> delay_dist(0, config.max_startup_delay_ms);
    
    int startup_delay = delay_dist(gen);
    printf("[WORKER %d] Random startup delay: %dms\n", worker_id, startup_delay);
    usleep(startup_delay * 1000);
    
    // Initialize application
    int fake_argc = 1;
    char* fake_argv[] = {const_cast<char*>("worker"), nullptr};
    auto appinit = std::make_shared<AppInitData>(fake_argc, fake_argv);
    appinit->_update_rendersync = false;
    appinit->_enable_graphics = false;
    appinit->_enable_audio = false;
    OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
    initModule(appinit);
    appinit->executePostInitOps();
    
    try {
        printf("[WORKER %d] Connecting to shared memory...\n", worker_id);
        
        // PHASE 1: CREATE/OPEN - Test atomic competition
        auto shared_mem = ShmObject<SharedTestData>::realize(config.shm_name);
        SharedTestData* data = shared_mem->image();
        
        // Check if we're the creator (initialization happens automatically)
        if (shared_mem->is_creator()) {
            printf("[WORKER %d] Winner! Initialized shared data\n", worker_id);
        }
        
        printf("[WORKER %d] Connected successfully (creator: %s)\n", 
               worker_id, shared_mem->is_creator() ? "YES" : "NO");
        
        // PHASE 2: COMPUTE - Announce arrival and do work
        uint64_t my_process_num = data->process_counter.fetch_add(1) + 1;
        printf("[WORKER %d] Joined as process #%llu\n", worker_id, my_process_num);
        
        // Do computational work with timing variations
        Timer work_timer;
        work_timer.Start();
        
        uint64_t my_work_done = 0;
        uint64_t my_checksum = 0;
        
        std::uniform_int_distribution<> work_delay_dist(1, 50); // 1-50ms between work items
        
        while (work_timer.SecsSinceStart() < (config.work_duration_ms / 1000.0) && 
               !data->shutdown_requested.load()) {
            
            // Get next work item atomically
            uint64_t work_idx = data->work_index.fetch_add(1);
            if (work_idx >= 1000) break; // Work array exhausted
            
            // Do expensive computation on work item
            uint64_t work_value = data->work_array[work_idx];
            for (int i = 0; i < 10000; ++i) { // 10k iterations per work item
                work_value = work_value * 1103515245ULL + 12345ULL;
                work_value ^= work_value >> 17;
                work_value *= 0x5deece66dULL;
                work_value ^= work_value >> 13;
            }
            
            // Update global counters atomically
            data->computation_counter.fetch_add(1);
            data->checksum.fetch_xor(work_value);
            
            my_work_done++;
            my_checksum ^= work_value;
            
            // Random delay between work items
            int work_delay = work_delay_dist(gen);
            usleep(work_delay * 1000);
        }
        
        printf("[WORKER %d] Work complete: %llu items, checksum=0x%llx\n", 
               worker_id, my_work_done, my_checksum);
        
        // PHASE 3: CLOSE - Coordinate shutdown
        if (data->shutdown_requested.load()) {
            printf("[WORKER %d] Shutdown requested, acknowledging...\n", worker_id);
            data->shutdown_counter.fetch_add(1);
        }
        
        printf("[WORKER %d] Exiting normally\n", worker_id);
        
    } catch (const std::exception& e) {
        printf("[WORKER %d] FATAL ERROR: %s\n", worker_id, e.what());
        _exit(1);
    } catch (...) {
        printf("[WORKER %d] FATAL ERROR: Unknown exception\n", worker_id);
        _exit(1);
    }
}

int main(int argc, char** argv) {
    SetCurrentThreadName("main");
    genviron.init_from_global_env();
    
    // Initialize application first
    auto appinit = std::make_shared<AppInitData>(argc, argv);
    appinit->_update_rendersync = false;
    appinit->_enable_graphics = false;
    appinit->_enable_audio = false;
    OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
    initModule(appinit);
    appinit->executePostInitOps();
    
    TestConfig config = parse_args(argc, argv);
    
    printf("=== ShmObject Stress Test ===\n");
    printf("Processes: %d\n", config.num_processes);
    printf("Work duration: %dms per process\n", config.work_duration_ms);
    printf("Max startup delay: %dms\n", config.max_startup_delay_ms);
    printf("Coordinator duration: %dms\n", config.coordinator_duration_ms);
    printf("Shared memory: %s\n", config.shm_name);
    printf("\n");
    
    // Clean up any leftover shared memory
    boost::interprocess::shared_memory_object::remove(config.shm_name);
    
    std::vector<pid_t> children;
    Timer test_timer;
    test_timer.Start();
    
    // Fork all worker processes with staggered timing
    for (int i = 0; i < config.num_processes; ++i) {
        pid_t child_pid = fork();
        
        if (child_pid == 0) {
            // Child process
            worker_process(i, config);
            _exit(0);
            
        } else if (child_pid > 0) {
            // Parent process
            children.push_back(child_pid);
            printf("[COORDINATOR] Forked worker %d (PID: %d)\n", i, child_pid);
            
            // Small delay between forks to create timing variation
            usleep(10000); // 10ms
            
        } else {
            perror("Failed to fork worker");
            // Kill existing children
            for (pid_t pid : children) {
                kill(pid, SIGTERM);
            }
            return 1;
        }
    }
    
    printf("[COORDINATOR] All %d workers forked, monitoring...\n", config.num_processes);
    
    // Monitor shared memory state
    try {
        auto shared_mem = ShmObject<SharedTestData>::realize(config.shm_name);
        SharedTestData* data = shared_mem->image();
        
        // Check if we're the creator (initialization happens automatically)
        if (shared_mem->is_creator()) {
            printf("[COORDINATOR] Winner! Initialized shared data\n");
        }
        
        // Monitor for specified duration
        while (test_timer.SecsSinceStart() < (config.coordinator_duration_ms / 1000.0)) {
            uint64_t processes = data->process_counter.load();
            uint64_t computations = data->computation_counter.load();
            uint64_t checksum = data->checksum.load();
            uint64_t work_index = data->work_index.load();
            
            printf("[COORDINATOR] Processes: %llu, Computations: %llu, Work index: %llu, Checksum: 0x%llx\n",
                   processes, computations, work_index, checksum);
            
            sleep(1);
        }
        
        // Request shutdown
        printf("[COORDINATOR] Requesting shutdown...\n");
        data->shutdown_requested.store(true);
        
        // Wait a bit for acknowledgments
        sleep(1);
        uint64_t shutdown_acks = data->shutdown_counter.load();
        printf("[COORDINATOR] Shutdown acknowledgments: %llu\n", shutdown_acks);
        
    } catch (const std::exception& e) {
        printf("[COORDINATOR] Shared memory error: %s\n", e.what());
    }
    
    // Wait for all children
    printf("[COORDINATOR] Waiting for all workers to complete...\n");
    
    int success_count = 0;
    int failure_count = 0;
    
    for (size_t i = 0; i < children.size(); ++i) {
        int status = 0;
        pid_t result = waitpid(children[i], &status, 0);
        
        if (result == children[i]) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                success_count++;
            } else {
                failure_count++;
                printf("[COORDINATOR] Worker PID %d failed with exit code %d\n", 
                       children[i], WEXITSTATUS(status));
            }
        } else {
            failure_count++;
            printf("[COORDINATOR] Failed to wait for worker PID %d\n", children[i]);
        }
    }
    
    printf("\n=== Test Results ===\n");
    printf("Total runtime: %.2f seconds\n", test_timer.SecsSinceStart());
    printf("Workers succeeded: %d\n", success_count);
    printf("Workers failed: %d\n", failure_count);
    
    if (failure_count == 0) {
        printf("\n🎉 SHARED MEMORY STRESS TEST PASSED!\n");
        printf("All processes successfully coordinated through shared memory\n");
        return 0;
    } else {
        printf("\n❌ SHARED MEMORY STRESS TEST FAILED!\n");
        printf("%d processes encountered errors\n", failure_count);
        return 1;
    }
}