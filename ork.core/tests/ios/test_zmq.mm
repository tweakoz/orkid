////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>
#include <zmq.h>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

using namespace ork;

////////////////////////////////////////////////////////////////
// ZeroMQ Request/Reply Test (TCP)
////////////////////////////////////////////////////////////////

void zmq_reqrep_test(logchannel_ptr_t logchan) {
    logchan->log("");
    logchan->log("--- ZeroMQ Request/Reply Test (TCP) ---");

    std::atomic<bool> server_ready{false};
    std::atomic<bool> test_complete{false};

    // Server thread
    auto server_op = [&](anyp) {
        void* context = zmq_ctx_new();
        void* responder = zmq_socket(context, ZMQ_REP);
        int rc = zmq_bind(responder, "tcp://127.0.0.1:5555");

        if (rc == 0) {
            logchan->log("Server: Bound to tcp://127.0.0.1:5555");
            server_ready = true;

            for (int i = 0; i < 5; i++) {
                char buffer[256];
                int size = zmq_recv(responder, buffer, 255, 0);
                if (size > 0) {
                    buffer[size] = '\0';
                    logchan->log("Server: Received \"%s\"", buffer);

                    std::string reply = "Reply " + std::to_string(i + 1);
                    zmq_send(responder, reply.c_str(), reply.length(), 0);
                    logchan->log("Server: Sent \"%s\"", reply.c_str());
                }
            }
        } else {
            logchan->log("Server: Failed to bind (error %d)", zmq_errno());
        }

        zmq_close(responder);
        zmq_ctx_destroy(context);
        logchan->log("Server: Shutdown complete");
    };

    // Client thread
    auto client_op = [&](anyp) {
        // Wait for server to be ready
        while (!server_ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        void* context = zmq_ctx_new();
        void* requester = zmq_socket(context, ZMQ_REQ);
        int rc = zmq_connect(requester, "tcp://127.0.0.1:5555");

        if (rc == 0) {
            logchan->log("Client: Connected to tcp://127.0.0.1:5555");

            for (int i = 0; i < 5; i++) {
                std::string request = "Request " + std::to_string(i + 1);
                zmq_send(requester, request.c_str(), request.length(), 0);
                logchan->log("Client: Sent \"%s\"", request.c_str());

                char buffer[256];
                int size = zmq_recv(requester, buffer, 255, 0);
                if (size > 0) {
                    buffer[size] = '\0';
                    logchan->log("Client: Received \"%s\"", buffer);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            logchan->log("Client: Failed to connect (error %d)", zmq_errno());
        }

        zmq_close(requester);
        zmq_ctx_destroy(context);
        logchan->log("Client: Shutdown complete");
        test_complete = true;
    };

    // Launch server and client threads
    ork::Thread server_thread("zmq_server");
    ork::Thread client_thread("zmq_client");

    server_thread.start(server_op);
    client_thread.start(client_op);

    // Wait for test to complete
    while (!test_complete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server_thread.join();
    client_thread.join();

    logchan->log("✓ Request/Reply test complete");
}

////////////////////////////////////////////////////////////////
// ZeroMQ Pub/Sub Test (inproc)
////////////////////////////////////////////////////////////////

void zmq_pubsub_test(logchannel_ptr_t logchan) {
    logchan->log("");
    logchan->log("--- ZeroMQ Pub/Sub Test (inproc) ---");

    // Shared context for inproc
    void* shared_context = zmq_ctx_new();
    std::atomic<bool> publisher_ready{false};
    std::atomic<int> messages_received{0};

    // Publisher thread
    auto publisher_op = [&](anyp) {
        void* publisher = zmq_socket(shared_context, ZMQ_PUB);
        int rc = zmq_bind(publisher, "inproc://pubsub");

        if (rc == 0) {
            logchan->log("Publisher: Bound to inproc://pubsub");
            publisher_ready = true;

            // Give subscriber time to connect
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            for (int i = 0; i < 10; i++) {
                std::string topic = (i % 2 == 0) ? "EVEN" : "ODD";
                std::string message = topic + " " + std::to_string(i);

                // Send topic first
                zmq_send(publisher, topic.c_str(), topic.length(), ZMQ_SNDMORE);
                // Then message
                zmq_send(publisher, message.c_str(), message.length(), 0);

                logchan->log("Publisher: Sent [%s] \"%s\"", topic.c_str(), message.c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Give subscribers time to receive
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            logchan->log("Publisher: Failed to bind (error %d)", zmq_errno());
        }

        zmq_close(publisher);
        logchan->log("Publisher: Shutdown complete");
    };

    // Subscriber thread (subscribes to EVEN messages)
    auto subscriber_op = [&](anyp) {
        // Wait for publisher
        while (!publisher_ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        void* subscriber = zmq_socket(shared_context, ZMQ_SUB);
        int rc = zmq_connect(subscriber, "inproc://pubsub");

        if (rc == 0) {
            // Subscribe to EVEN messages
            zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "EVEN", 4);
            logchan->log("Subscriber: Connected and subscribed to \"EVEN\"");

            // Receive messages
            for (int i = 0; i < 5; i++) {
                char topic[256];
                char message[256];

                // Receive topic
                int topic_size = zmq_recv(subscriber, topic, 255, 0);
                if (topic_size > 0) {
                    topic[topic_size] = '\0';

                    // Receive message
                    int msg_size = zmq_recv(subscriber, message, 255, 0);
                    if (msg_size > 0) {
                        message[msg_size] = '\0';
                        logchan->log("Subscriber: Received [%s] \"%s\"", topic, message);
                        messages_received++;
                    }
                }
            }
        } else {
            logchan->log("Subscriber: Failed to connect (error %d)", zmq_errno());
        }

        zmq_close(subscriber);
        logchan->log("Subscriber: Shutdown complete");
    };

    // Launch threads
    ork::Thread pub_thread("zmq_publisher");
    ork::Thread sub_thread("zmq_subscriber");

    pub_thread.start(publisher_op);
    sub_thread.start(subscriber_op);

    pub_thread.join();
    sub_thread.join();

    zmq_ctx_destroy(shared_context);

    logchan->log("✓ Pub/Sub test complete (received %d messages)", messages_received.load());
}

////////////////////////////////////////////////////////////////
// ZeroMQ Push/Pull Test (inproc)
////////////////////////////////////////////////////////////////

void zmq_pushpull_test(logchannel_ptr_t logchan) {
    logchan->log("");
    logchan->log("--- ZeroMQ Push/Pull Test (inproc) ---");

    void* shared_context = zmq_ctx_new();
    std::atomic<bool> puller_ready{false};
    std::atomic<int> total_work{0};

    // Puller (worker) thread
    auto puller_op = [&](anyp) {
        void* puller = zmq_socket(shared_context, ZMQ_PULL);
        int rc = zmq_bind(puller, "inproc://pushpull");

        if (rc == 0) {
            logchan->log("Puller: Bound to inproc://pushpull");
            puller_ready = true;

            for (int i = 0; i < 10; i++) {
                char buffer[256];
                int size = zmq_recv(puller, buffer, 255, 0);
                if (size > 0) {
                    buffer[size] = '\0';
                    int workload = atoi(buffer);
                    total_work += workload;
                    logchan->log("Puller: Received workload %d (total: %d)", workload, total_work.load());
                }
            }
        } else {
            logchan->log("Puller: Failed to bind (error %d)", zmq_errno());
        }

        zmq_close(puller);
        logchan->log("Puller: Shutdown complete");
    };

    // Pusher thread
    auto pusher_op = [&](anyp) {
        // Wait for puller
        while (!puller_ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        void* pusher = zmq_socket(shared_context, ZMQ_PUSH);
        int rc = zmq_connect(pusher, "inproc://pushpull");

        if (rc == 0) {
            logchan->log("Pusher: Connected to inproc://pushpull");

            for (int i = 1; i <= 10; i++) {
                std::string workload = std::to_string(i * 10);
                zmq_send(pusher, workload.c_str(), workload.length(), 0);
                logchan->log("Pusher: Sent workload %s", workload.c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Give puller time to finish
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            logchan->log("Pusher: Failed to connect (error %d)", zmq_errno());
        }

        zmq_close(pusher);
        logchan->log("Pusher: Shutdown complete");
    };

    // Launch threads
    ork::Thread pull_thread("zmq_puller");
    ork::Thread push_thread("zmq_pusher");

    pull_thread.start(puller_op);
    push_thread.start(pusher_op);

    pull_thread.join();
    push_thread.join();

    zmq_ctx_destroy(shared_context);

    logchan->log("✓ Push/Pull test complete (total work: %d)", total_work.load());
}

////////////////////////////////////////////////////////////////
// ZeroMQ Version and Info
////////////////////////////////////////////////////////////////

void zmq_version_test(logchannel_ptr_t logchan) {
    logchan->log("");
    logchan->log("--- ZeroMQ Version and Info ---");

    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    logchan->log("ZeroMQ Version: %d.%d.%d", major, minor, patch);

    // Test context creation and destruction
    void* ctx = zmq_ctx_new();
    if (ctx) {
        logchan->log("✓ Context created successfully");

        // Get context options
        int io_threads = zmq_ctx_get(ctx, ZMQ_IO_THREADS);
        int max_sockets = zmq_ctx_get(ctx, ZMQ_MAX_SOCKETS);

        logchan->log("IO Threads: %d", io_threads);
        logchan->log("Max Sockets: %d", max_sockets);

        zmq_ctx_destroy(ctx);
        logchan->log("✓ Context destroyed successfully");
    } else {
        logchan->log("✗ Failed to create context");
    }
}

////////////////////////////////////////////////////////////////
// Main ZeroMQ Test Runner
////////////////////////////////////////////////////////////////

void runZmqTests(void) {
    auto logchan = logger()->configureChannel("ZeroMQ", fvec3(0.4f, 0.8f, 0.4f), true);

    logchan->log("========================================");
    logchan->log("Starting ZeroMQ Tests");
    logchan->log("========================================");

    // Test 1: Version and Info
    zmq_version_test(logchan);

    // Test 2: Request/Reply over TCP
    zmq_reqrep_test(logchan);

    // Test 3: Pub/Sub over inproc
    zmq_pubsub_test(logchan);

    // Test 4: Push/Pull over inproc
    zmq_pushpull_test(logchan);

    logchan->log("");
    logchan->log("========================================");
    logchan->log("ZeroMQ Tests Complete");
    logchan->log("========================================");
}
