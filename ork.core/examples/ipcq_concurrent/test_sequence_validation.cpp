#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/util/ipcq.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/filedev.h>
#include <ork/file/path.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>

using namespace ork;

namespace ork {
  void initModule(appinitdata_ptr_t appinit);
  void exitModule(appinitdata_ptr_t appinit);
}

const int NUM_BLOCKS = 10;

void runProducer(const std::string& queue_name, uint64_t shared_seed) {
    printf("[PRODUCER] Starting producer process (PID: %d)\n", getpid());
    fflush(stdout);
    
    // Initialize application in child process
    int fake_argc = 1;
    char* fake_argv[] = {"producer", nullptr};
    auto appinit = std::make_shared<AppInitData>(fake_argc, fake_argv);
    appinit->_update_rendersync = false;
    appinit->_enable_graphics = false;
    appinit->_enable_audio = false;
    OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
    initModule(appinit);
    appinit->executePostInitOps();
    
    try {
        printf("[PRODUCER] Creating sender...\n");
        fflush(stdout);
        auto sender = std::make_shared<ipcq_8K1K_sender_t>(queue_name);
        printf("[PRODUCER] Sender created successfully\n");
        fflush(stdout);
        
        printf("[PRODUCER] Computing anti-spoofing sequence...\n");
        fflush(stdout);
        uint64_t cumulative_hash = shared_seed;  // Use shared seed as initial value
        
        // Send test messages with expensive computation
        for (int i = 0; i < NUM_BLOCKS; ++i) {
            typename ipcq_8K1K_sender_t::message_t msg;
            
            // Expensive computation to prevent spoofing
            uint64_t computed_value = i + shared_seed;
            for (int j = 0; j < 100000; ++j) {  // 100k iterations per message
                computed_value = computed_value * 1103515245ULL + 12345ULL;
                computed_value ^= computed_value >> 17;
                computed_value *= 0x5deece66dULL;
                computed_value ^= computed_value >> 13;
            }
            
            // Update cumulative hash with computed value
            cumulative_hash ^= computed_value;
            
            // Write data that proves computation was done
            msg.write<int>(i);
            msg.write<float>(i * 1.5f);
            msg.write<uint64_t>(computed_value);      // Proof of work
            msg.write<uint64_t>(cumulative_hash);     // Running verification
            
            printf("[PRODUCER] Sending message %d (computed: 0x%llx, cumulative: 0x%llx)...\n",
                   i, (ull)computed_value, (ull)cumulative_hash);
            fflush(stdout);
            sender->send(msg);
            
            usleep(100000); // 100ms between messages
        }
        
        printf("[PRODUCER] Producer finished successfully (final hash: 0x%llx)\n", (ull)cumulative_hash);
        fflush(stdout);
        
    } catch (const std::exception& e) {
        printf("[PRODUCER] FATAL ERROR: %s\n", e.what());
        fflush(stdout);
        _exit(1);
    } catch (...) {
        printf("[PRODUCER] FATAL ERROR: Unknown exception\n");
        fflush(stdout);
        _exit(1);
    }
}

void runConsumer(const std::string& queue_name, uint64_t shared_seed) {
    printf("[CONSUMER] Starting consumer process (PID: %d)\n", getpid());
    fflush(stdout);
    
    // Initialize application in child process
    int fake_argc = 1;
    char* fake_argv[] = {"consumer", nullptr};
    auto appinit = std::make_shared<AppInitData>(fake_argc, fake_argv);
    appinit->_update_rendersync = false;
    appinit->_enable_graphics = false;
    appinit->_enable_audio = false;
    OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
    initModule(appinit);
    appinit->executePostInitOps();
    
    try {
        printf("[CONSUMER] Creating receiver...\n");
        fflush(stdout);
        auto receiver = std::make_shared<ipcq_8K1K_receiver_t>(queue_name);
        printf("[CONSUMER] Receiver created successfully\n");
        fflush(stdout);
        printf("[CONSUMER] Ready to receive and verify!\n");
        fflush(stdout);
        
        uint64_t expected_cumulative = shared_seed;  // Use shared seed as initial value
        int verification_errors = 0;
        
        // Receive messages and verify computation
        int received = 0;
        while (received < NUM_BLOCKS) {
            typename ipcq_8K1K_receiver_t::message_t msg;
            if (receiver->tryReceive(msg)) {
                int value = 0;
                float fvalue = 0.0f;
                uint64_t received_computed = 0;
                uint64_t received_cumulative = 0;
                
                msg.read(value);
                msg.read(fvalue);
                msg.read(received_computed);
                msg.read(received_cumulative);
                
                // Verify the computation independently
                printf("[CONSUMER] Verifying message %d...\n", value);
                fflush(stdout);
                uint64_t expected_computed = value + shared_seed;
                for (int j = 0; j < 100000; ++j) {  // Same expensive computation
                    expected_computed = expected_computed * 1103515245ULL + 12345ULL;
                    expected_computed ^= expected_computed >> 17;
                    expected_computed *= 0x5deece66dULL;
                    expected_computed ^= expected_computed >> 13;
                }
                
                // Update expected cumulative
                expected_cumulative ^= expected_computed;
                
                // Verify both values match
                if (received_computed != expected_computed) {
                    printf("[CONSUMER] ❌ VERIFICATION FAILED: Message %d computed value mismatch!\n", value);
                    printf("[CONSUMER]    Expected: 0x%llx, Received: 0x%llx\n", (ull)expected_computed, (ull)received_computed);
                    fflush(stdout);
                    verification_errors++;
                } else if (received_cumulative != expected_cumulative) {
                    printf("[CONSUMER] ❌ VERIFICATION FAILED: Message %d cumulative hash mismatch!\n", value);
                    printf("[CONSUMER]    Expected: 0x%llx, Received: 0x%llx\n", (ull)expected_cumulative, (ull)received_cumulative);
                    fflush(stdout);
                    verification_errors++;
                } else {
                    printf("[CONSUMER] ✅ Verified message: int=%d, float=%.2f, hash=0x%llx\n",
                           value, fvalue, (ull)received_computed);
                    fflush(stdout);
                }
                
                received++;
            } else {
                usleep(10000); // 10ms
            }
        }
        
        if (verification_errors == 0) {
            printf("[CONSUMER] ✅ Consumer finished successfully - All computations verified!\n");
            fflush(stdout);
        } else {
            printf("[CONSUMER] ❌ Consumer detected %d verification errors - Test FAILED!\n", verification_errors);
            fflush(stdout);
            exit(1);
        }
        
    } catch (const std::exception& e) {
        printf("[CONSUMER] FATAL ERROR: %s\n", e.what());
        fflush(stdout);
        _exit(1);
    } catch (...) {
        printf("[CONSUMER] FATAL ERROR: Unknown exception\n");
        fflush(stdout);
        _exit(1);
    }
}

int main(int argc, char** argv) {
    SetCurrentThreadName("main");
    genviron.init_from_global_env();
    
    // Generate unique seed for this test run
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    uint64_t shared_seed = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    
    const std::string queue_name = "sequence_validation_test";
            
    printf("=== IPCQ Anti-Spoofing Sequence Validation Test ===\n");
    printf("Using expensive computation for validation\n");
    printf("Queue: %s\n", queue_name.c_str());
    printf("Message size: 8KB, Queue size: 1K entries\n");
    printf("Test seed: 0x%llx (unique for this run)\n", (ull)shared_seed);
    printf("Test length: %d blocks\n\n", NUM_BLOCKS);
    
    // Fork to create consumer and producer processes
    // Consumer should start first to create the shared memory
    pid_t consumer_pid = fork();
    
    if (consumer_pid == 0) {
        // Child process - consumer (starts first, creates queue)
        runConsumer(queue_name, shared_seed);
        _exit(0);

    } else if (consumer_pid > 0) {
        // Parent process will fork again for producer
        // Give consumer time to fully initialize and create the queue
        usleep(1000000); // 1 second delay to let consumer initialize
        
        pid_t producer_pid = fork();
        
        if (producer_pid == 0) {
            // Child process - producer (starts second, opens existing queue)
            runProducer(queue_name, shared_seed);
            _exit(0);
            
        } else if (producer_pid > 0) {
            // Parent process - wait for both children
            printf("[MAIN] Waiting for consumer (PID: %d) and producer (PID: %d)\n\n", consumer_pid, producer_pid);
            
            int producer_status = 0;
            int consumer_status = 0;
            
            // Wait for both processes
            waitpid(consumer_pid, &consumer_status, 0);
            waitpid(producer_pid, &producer_status, 0);
            
            printf("\n=== Test Results ===\n");
            
            bool producer_success = WIFEXITED(producer_status) && WEXITSTATUS(producer_status) == 0;
            bool consumer_success = WIFEXITED(consumer_status) && WEXITSTATUS(consumer_status) == 0;
            
            printf("Producer: %s\n", producer_success ? "SUCCESS" : "FAILED");
            printf("Consumer: %s\n", consumer_success ? "SUCCESS" : "FAILED");
            
            if (producer_success && consumer_success) {
                printf("\n🎉 ANTI-SPOOFING SEQUENCE VALIDATION TEST PASSED!\n");
                printf("All data transmitted correctly with independent verification\n");
                return 0;
            } else {
                printf("\n❌ ANTI-SPOOFING SEQUENCE VALIDATION TEST FAILED!\n");
                if (!producer_success) {
                    printf("Producer exit code: %d\n", WEXITSTATUS(producer_status));
                }
                if (!consumer_success) {
                    printf("Consumer exit code: %d\n", WEXITSTATUS(consumer_status));
                }
                return 1;
            }
            
        } else {
            perror("Failed to fork producer");
            kill(consumer_pid, SIGTERM);
            waitpid(consumer_pid, nullptr, 0);
            return 1;
        }
        
    } else {
        perror("Failed to fork consumer");
        return 1;
    }
}