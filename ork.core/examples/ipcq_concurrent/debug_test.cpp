#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/util/ipcq.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/filedev.h>
#include <ork/file/path.h>
#include <ork/kernel/opq.h>
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
  const std::string queue_name = "debug_test";
  
  // Initialize application
  auto appinit = std::make_shared<AppInitData>(argc, argv);
  appinit->_update_rendersync = false;
  appinit->_enable_graphics = false;
  appinit->_enable_audio = false;
  
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());
  initModule(appinit);
  appinit->executePostInitOps();
  
  printf("Starting %s mode for queue '%s'\n", mode.c_str(), queue_name.c_str());
  printf("Queue size: 8192, Message size: %zu\n", 
         ipcq_8K1K_sender_t::message_t::kMaxSize);
  
  if (mode == "producer") {
    printf("Creating sender...\n");
    auto sender = std::make_shared<ipcq_8K1K_sender_t>(queue_name);
    printf("Sender ready\n");
    
    // Send messages and count
    int sent = 0;
    const int total_messages = 8<<20;
    
    printf("Sending %d data messages...\n", total_messages);
    Timer send_timer;
    send_timer.Start();
    
    for (int i = 0; i < total_messages; ++i) {
      typename ipcq_8K1K_sender_t::message_t msg;
      msg.write<int>(i);
      
      sender->send(msg);
      sent++;
    }
    
    double send_time = send_timer.SecsSinceStart();
    double send_rate = sent / send_time;
    size_t bytes_sent = sent * sizeof(int); // Each message contains one int
    double mib_per_sec = (bytes_sent / (1024.0 * 1024.0)) / send_time;
    printf("Producer finished. Sent %d messages in %.2f seconds\n", sent, send_time);
    printf("  Performance: %.0f msgs/sec, %.2f MiB/sec\n", send_rate, mib_per_sec);
    
    // Send end message
    printf("Sending END message...\n");
    typename ipcq_8K1K_sender_t::message_t end_msg;
    end_msg.write<int>(-1); // Special end marker
    sender->send(end_msg);
    
    printf("Producer completed successfully.\n");
    
    // BANDWIDTH TEST - Full sized packets
    printf("\n=== Starting Bandwidth Test (Full-sized packets) ===\n");
    const int bandwidth_messages = 100000; // Less messages but full size
    printf("Sending %d full-sized messages (%zu bytes each)...\n", 
           bandwidth_messages, ipcq_8K1K_sender_t::message_t::kMaxSize);
    
    // Pre-create a full-sized message with pattern
    typename ipcq_8K1K_sender_t::message_t full_msg;
    uint8_t pattern[ipcq_8K1K_sender_t::message_t::kMaxSize];
    for (size_t i = 0; i < sizeof(pattern); ++i) {
      pattern[i] = (uint8_t)(i & 0xFF);
    }
    full_msg.writeData(pattern, sizeof(pattern));
    
    Timer bandwidth_timer;
    bandwidth_timer.Start();
    
    for (int i = 0; i < bandwidth_messages; ++i) {
      sender->send(full_msg);
    }
    
    double bandwidth_time = bandwidth_timer.SecsSinceStart();
    size_t total_bytes = bandwidth_messages * ipcq_8K1K_sender_t::message_t::kMaxSize;
    double bandwidth_mib_per_sec = (total_bytes / (1024.0 * 1024.0)) / bandwidth_time;
    double msgs_per_sec = bandwidth_messages / bandwidth_time;
    
    printf("Bandwidth test completed: %d messages in %.2f seconds\n", 
           bandwidth_messages, bandwidth_time);
    printf("  Throughput: %.2f MiB/sec (%.0f msgs/sec)\n", bandwidth_mib_per_sec, msgs_per_sec);
    printf("  Total data: %.2f MiB\n", total_bytes / (1024.0 * 1024.0));
    
    // Send special end marker for bandwidth test
    typename ipcq_8K1K_sender_t::message_t end_bandwidth_msg;
    end_bandwidth_msg.write<int>(-2); // Special bandwidth end marker
    sender->send(end_bandwidth_msg);
    
  } else if (mode == "consumer") {
    printf("Creating receiver...\n");
    auto receiver = std::make_shared<ipcq_8K1K_receiver_t>(queue_name);
    printf("Receiver ready\n");
    
    // Receive messages
    int received = 0;
    int attempts = 0;
    
    printf("Receiving data messages until END marker...\n");
    Timer recv_timer;
    recv_timer.Start();
    
    bool end_received = false;
    while (!end_received && attempts < 10000000) {
      typename ipcq_8K1K_receiver_t::message_t msg;
      if (receiver->tryReceive(msg)) {
        int value = 0;
        msg.read(value);
        
        if (value == -1) {
          printf("Received END message, stopping...\n");
          end_received = true;
        } else {
          received++;
        }
      } else {
        usleep(10); // 0.01ms - same as bandwidth test
        attempts++;
      }
    }
    
    double recv_time = recv_timer.SecsSinceStart();
    double recv_rate = received / recv_time;
    size_t bytes_received = received * sizeof(int); // Each message contains one int
    double mib_per_sec = (bytes_received / (1024.0 * 1024.0)) / recv_time;
    double success_rate = (attempts > 0) ? (double)received * 100.0 / attempts : 0.0;
    
    printf("Consumer finished. Received %d messages in %.2f seconds\n", received, recv_time);
    printf("  Performance: %.0f msgs/sec, %.2f MiB/sec\n", recv_rate, mib_per_sec);
    printf("  Success rate: %.2f%% (%d successful / %d attempts)\n", 
           success_rate, received, attempts);
    
    // BANDWIDTH TEST - Receive full sized packets
    printf("\n=== Starting Bandwidth Test Reception ===\n");
    int bandwidth_received = 0;
    Timer bandwidth_timer;
    bandwidth_timer.Start();
    
    // Pre-allocate buffer for verification (optional)
    uint8_t expected_pattern[ipcq_8K1K_receiver_t::message_t::kMaxSize];
    for (size_t i = 0; i < sizeof(expected_pattern); ++i) {
      expected_pattern[i] = (uint8_t)(i & 0xFF);
    }
    
    bool bandwidth_end = false;
    while (!bandwidth_end) {
      typename ipcq_8K1K_receiver_t::message_t msg;
      if (receiver->tryReceive(msg)) {
        // Check if it's the end marker (first int will be -2)
        // Small messages are likely end markers
        if (msg.length() <= sizeof(int)) {
          int value = 0;
          msg.read(value);
          if (value == -2) {
            printf("Received bandwidth test END marker\n");
            bandwidth_end = true;
            continue;
          }
        }
        bandwidth_received++;
      } else {
        usleep(10); // 0.01ms - shorter sleep for bandwidth test
      }
    }
    
    double bandwidth_time = bandwidth_timer.SecsSinceStart();
    size_t total_bytes = bandwidth_received * ipcq_8K1K_receiver_t::message_t::kMaxSize;
    double bandwidth_mib_per_sec = (total_bytes / (1024.0 * 1024.0)) / bandwidth_time;
    double bandwidth_msgs_per_sec = bandwidth_received / bandwidth_time;
    
    printf("Bandwidth test completed: Received %d full-sized messages in %.2f seconds\n", 
           bandwidth_received, bandwidth_time);
    printf("  Throughput: %.2f MiB/sec (%.0f msgs/sec)\n", bandwidth_mib_per_sec, bandwidth_msgs_per_sec);
    printf("  Total data: %.2f MiB\n", total_bytes / (1024.0 * 1024.0));
    
    // Destructor will handle cleanup
    
  } else {
    printf("Unknown mode: %s\n", mode.c_str());
    return 1;
  }
  exitModule(appinit);
  return 0;
}