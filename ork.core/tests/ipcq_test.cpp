////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/util/ipcq.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/datacache.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

using namespace ork;

// Type aliases for legacy compatibility
using ipcq_message_t = IpcMessagePacket<256>;
using ipcq_message_iterator_t = IpcMessagePacketIterator<256>;
static constexpr size_t PRIMARY_QUEUE_SIZE = 8192;

static constexpr bool VERBOSE_TESTS = true;

///////////////////////////////////////////////////////////////////////////////
// Test 1: Basic single-threaded send/receive
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_basic_send_receive) {
    
    
    // Create sender
    auto sender = std::make_shared<ipcq_sender_t>("test_ipcq_basic");
    
    // Create receiver  
    auto receiver = std::make_shared<ipcq_receiver_t>("test_ipcq_basic");
    
    // Send test message
    ipcq_message_t msg_out;
    msg_out.writeString("hello");
    msg_out.write<int>(42);
    msg_out.write<float>(3.14f);
    sender->send(msg_out);
    
    // Receive and verify
    ipcq_message_t msg_in;
    bool received = receiver->tryReceive(msg_in);
    CHECK(received == true);
    
    auto iter = msg_in.makeIterator();
    std::string str = msg_in.readString(iter);
    CHECK(str == "hello");
    
    int ival = 0;
    msg_in.read(ival, iter);
    CHECK(ival == 42);
    
    float fval = 0.0f;
    msg_in.read(fval, iter);
    CHECK(fabs(fval - 3.14f) < 0.001f);
    
    if(VERBOSE_TESTS) printf("Test 1: Basic send/receive passed\n");
}

///////////////////////////////////////////////////////////////////////////////
// Test 2: Multi-threaded producer/consumer
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_multithread_producer_consumer) {
    
    static constexpr int NUM_MESSAGES = 1000;
    static constexpr int NUM_PRODUCERS = 3;
    
    auto sender = std::make_shared<ipcq_sender_t>("test_ipcq_multithread");
    
    std::atomic<int> messages_sent(0);
    std::atomic<int> messages_received(0);
    
    // Producer threads
    std::vector<thread_ptr_t> producers;
    for(int p = 0; p < NUM_PRODUCERS; p++) {
        auto producer = std::make_shared<Thread>(FormatString("producer_%d", p));
        producer->start([sender, p, &messages_sent](anyp data) {
            for(int i = 0; i < NUM_MESSAGES; i++) {
                ipcq_message_t msg;
                msg.writeString("producer");
                msg.write<int>(p);
                msg.write<int>(i);
                sender->send(msg);
                messages_sent++;
                if(i % 100 == 0) usleep(1000); // Slight delay to test queuing
            }
            if(VERBOSE_TESTS) printf("Producer %d completed\n", p);
        });
        producers.push_back(producer);
    }
    
    // Consumer thread
    std::atomic<bool> consumer_error(false);
    auto consumer = std::make_shared<Thread>("consumer");
    consumer->start([&messages_received, &consumer_error](anyp data) {
        auto receiver = std::make_shared<ipcq_receiver_t>("test_ipcq_multithread");
        
        int expected_total = NUM_PRODUCERS * NUM_MESSAGES;
        
        while(messages_received < expected_total) {
            ipcq_message_t msg;
            if(receiver->tryReceive(msg)) {
                auto iter = msg.makeIterator();
                std::string type = msg.readString(iter);
                if(type != "producer") {
                    consumer_error = true;
                    break;
                }
                
                int producer_id = 0;
                msg.read(producer_id, iter);
                if(producer_id < 0 || producer_id >= NUM_PRODUCERS) {
                    consumer_error = true;
                    break;
                }
                
                messages_received++;
            } else {
                usleep(100);
            }
        }
        if(VERBOSE_TESTS) printf("Consumer received all %d messages\n", messages_received.load());
    });
    
    // Wait for completion
    for(auto& p : producers) {
        p->join();
    }
    consumer->join();
    
    CHECK(!consumer_error);
    CHECK(messages_sent == NUM_PRODUCERS * NUM_MESSAGES);
    CHECK(messages_received == NUM_PRODUCERS * NUM_MESSAGES);
    
    if(VERBOSE_TESTS) printf("Test 2: Multi-threaded producer/consumer passed\n");
}

///////////////////////////////////////////////////////////////////////////////
// Test 3: Large data transfer via DataBlock
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_datablock_transfer) {
    
    auto sender = std::make_shared<ipcq_sender_t>("test_ipcq_datablock");
    
    auto receiver = std::make_shared<ipcq_receiver_t>("test_ipcq_datablock");
    
    // Create test data (1MB)
    static constexpr size_t TEST_SIZE = 1024 * 1024;
    auto send_block = std::make_shared<DataBlock>();
    std::vector<uint8_t> test_data(TEST_SIZE);
    
    // Fill with pattern
    for(size_t i = 0; i < TEST_SIZE; i++) {
        test_data[i] = (i * 37) & 0xFF;
    }
    send_block->addData(test_data.data(), TEST_SIZE);
    
    // Send
    Timer send_timer;
    send_timer.Start();
    sender->sendDataBlock(send_block);
    double send_time = send_timer.SecsSinceStart();
    
    // Receive
    Timer recv_timer;
    recv_timer.Start();
    auto recv_block = receiver->receiveDataBlock();
    double recv_time = recv_timer.SecsSinceStart();
    
    // Verify
    CHECK(recv_block != nullptr);
    CHECK(recv_block->length() == TEST_SIZE);
    
    const uint8_t* recv_data = (const uint8_t*)recv_block->data();
    for(size_t i = 0; i < TEST_SIZE; i++) {
        CHECK(recv_data[i] == ((i * 37) & 0xFF));
    }
    
    double bandwidth_mbps = (TEST_SIZE / (1024.0 * 1024.0)) / send_time;
    if(VERBOSE_TESTS) {
        printf("Test 3: DataBlock transfer passed\n");
        printf("  Size: %zu bytes\n", TEST_SIZE);
        printf("  Send time: %.3f ms\n", send_time * 1000);
        printf("  Recv time: %.3f ms\n", recv_time * 1000);
        printf("  Bandwidth: %.1f MB/s\n", bandwidth_mbps);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Test 4: Performance benchmark
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_performance_benchmark) {
    
    auto sender = std::make_shared<ipcq_sender_t>("test_ipcq_perf");
    
    static constexpr int NUM_MESSAGES = 10000;
    
    // Prepare messages
    std::vector<ipcq_message_t> messages;
    for(int i = 0; i < NUM_MESSAGES; i++) {
        ipcq_message_t msg;
        msg.write<uint64_t>(i);
        msg.write<double>(Timer::get_sync_time());
        // Fill to ~200 bytes
        char filler[180];
        memset(filler, i & 0xFF, sizeof(filler));
        msg.writeData(filler, sizeof(filler));
        messages.push_back(msg);
    }
    
    // Start receiver thread
    std::atomic<int> received(0);
    std::atomic<double> total_latency(0.0);
    std::atomic<bool> receiver_error(false);
    
    auto receiver_thread = std::make_shared<Thread>("perf_receiver");
    receiver_thread->start([&received, &total_latency, &receiver_error](anyp data) {
        auto receiver = std::make_shared<ipcq_receiver_t>("test_ipcq_perf");
        
        Timer recv_timer;
        recv_timer.Start();
        
        while(received < NUM_MESSAGES) {
            ipcq_message_t msg;
            if(receiver->tryReceive(msg)) {
                auto iter = msg.makeIterator();
                uint64_t seq = 0;
                msg.read(seq, iter);
                
                if(seq != received) {
                    receiver_error = true;
                    break;
                }
                
                double send_timestamp = 0.0;
                msg.read(send_timestamp, iter);
                double latency = Timer::get_sync_time() - send_timestamp;
                
                // Atomic addition for double is tricky, just track count for now
                received++;
            } else {
                usleep(10); // Small sleep to avoid busy waiting
            }
        }
    });
    
    // Give receiver time to start
    usleep(10000);
    
    // Benchmark send
    Timer send_timer;
    send_timer.Start();
    for(const auto& msg : messages) {
        sender->send(msg);
    }
    double send_time = send_timer.SecsSinceStart();
    
    // Wait for receiver to finish
    receiver_thread->join();
    
    CHECK(!receiver_error);
    CHECK(received == NUM_MESSAGES);
    
    double send_rate = NUM_MESSAGES / send_time;
    
    if(VERBOSE_TESTS) {
        printf("Test 4: Performance benchmark\n");
        printf("  Messages: %d\n", NUM_MESSAGES);
        printf("  Send time: %.3f sec\n", send_time);
        printf("  Send rate: %.0f msg/sec\n", send_rate);
    }
    
    // Performance expectations
    CHECK(send_rate > 10000);  // >10k messages/sec
}


///////////////////////////////////////////////////////////////////////////////
// Test 6: Inter-process communication (fork test)
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_interprocess_fork) {
    
    pid_t pid = fork();
    
    if(pid == 0) {
        // Child process - receiver
        // In child, we need to be careful with thread cleanup
        // Use raw pointer and manual cleanup to avoid thread issues
        ipcq_receiver_t* receiver = new ipcq_receiver_t("test_ipcq_fork");
        
        int count = 0;
        while(count < 10) {
            ipcq_message_t msg;
            if(receiver->tryReceive(msg)) {
                auto iter = msg.makeIterator();
                std::string str = msg.readString(iter);
                int val = 0;
                msg.read(val, iter);
                
                if(VERBOSE_TESTS) printf("Child received: %s %d\n", str.c_str(), val);
                count++;
            } else {
                usleep(1000);
            }
        }
        
        // Don't delete receiver - just exit
        // This avoids thread join issues in forked process
        _exit(0); // Use _exit instead of exit to avoid atexit handlers
        
    } else {
        // Parent process - sender
        usleep(100000); // Let child start
        
        auto sender = std::make_shared<ipcq_sender_t>("test_ipcq_fork");
        
        for(int i = 0; i < 10; i++) {
            ipcq_message_t msg;
            msg.writeString("message");
            msg.write<int>(i);
            sender->send(msg);
            usleep(10000);
        }
        
        // Wait for child
        int status;
        waitpid(pid, &status, 0);
        CHECK(WIFEXITED(status));
        CHECK(WEXITSTATUS(status) == 0);
        
        if(VERBOSE_TESTS) printf("Test 6: Inter-process communication passed\n");
    }
}