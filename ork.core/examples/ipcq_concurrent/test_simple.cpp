#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/util/ipcq.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/filedev.h>
#include <ork/file/path.h>
#include <stdio.h>

using namespace ork;

namespace ork {
  void initModule(appinitdata_ptr_t appinit);
  void exitModule(appinitdata_ptr_t appinit);
}

int main(int argc, char** argv) {
  SetCurrentThreadName("main");
  genviron.init_from_global_env();
  
  if (argc < 2) {
    printf("Usage: %s [producer|consumer]\n", argv[0]);
    return 1;
  }
  
  const std::string mode = argv[1];
  const std::string queue_name = "simple_test";
  
  // Initialize application
  auto appinit = std::make_shared<AppInitData>(argc, argv);
  appinit->_update_rendersync = false;
  appinit->_enable_graphics = false;
  appinit->_enable_audio = false;
  
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
  initModule(appinit);
  appinit->executePostInitOps();
  
  printf("Starting %s mode for queue '%s'\n", mode.c_str(), queue_name.c_str());
  
  if (mode == "producer") {
    try {
      printf("Creating sender...\n");
      auto sender = std::make_shared<ipcq_8K1K_sender_t>(queue_name);
      
      printf("Computing anti-spoofing sequence...\n");
      uint64_t cumulative_hash = 12345;  // Initial seed
      
      // Send a few test messages with expensive computation
      for (int i = 0; i < 10; ++i) {
        typename ipcq_8K1K_sender_t::message_t msg;
        
        // Expensive computation to prevent spoofing
        uint64_t computed_value = i;
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
        
        printf("Sending message %d (computed: 0x%llx, cumulative: 0x%llx)...\n",
               i, (ull)computed_value, (ull)cumulative_hash);
        sender->send(msg);
        
        usleep(100000); // 100ms between messages
      }
      
      printf("Producer finished successfully (final hash: 0x%llx)\n", (ull)cumulative_hash);
      
    } catch (const std::exception& e) {
      printf("Producer error: %s\n", e.what());
      return 1;
    }
    
  } else if (mode == "consumer") {
    try {
      printf("Creating receiver...\n");
      auto receiver = std::make_shared<ipcq_8K1K_receiver_t>(queue_name);
      printf("Ready to receive and verify!\n");
      
      uint64_t expected_cumulative = 12345;  // Initial seed
      int verification_errors = 0;
      
      // Receive messages and verify computation
      int received = 0;
      while (received < 10) {
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
          printf("Verifying message %d...\n", value);
          uint64_t expected_computed = value;
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
            printf("❌ VERIFICATION FAILED: Message %d computed value mismatch!\n", value);
            printf("   Expected: 0x%llx, Received: 0x%llx\n", (ull)expected_computed, (ull)received_computed);
            verification_errors++;
          } else if (received_cumulative != expected_cumulative) {
            printf("❌ VERIFICATION FAILED: Message %d cumulative hash mismatch!\n", value);
            printf("   Expected: 0x%llx, Received: 0x%llx\n", (ull)expected_cumulative, (ull)received_cumulative);
            verification_errors++;
          } else {
            printf("✅ Verified message: int=%d, float=%.2f, hash=0x%llx\n",
                   value, fvalue, (ull)received_computed);
          }
          
          received++;
        } else {
          usleep(10000); // 10ms
        }
      }
      
      if (verification_errors == 0) {
        printf("✅ Consumer finished successfully - All computations verified!\n");
      } else {
        printf("❌ Consumer detected %d verification errors - Test FAILED!\n", verification_errors);
        return 1;
      }
      
    } catch (const std::exception& e) {
      printf("Consumer error: %s\n", e.what());
      return 1;
    }
  } else {
    printf("Unknown mode: %s\n", mode.c_str());
    return 1;
  }
  
  exitModule(appinit);
  return 0;
}