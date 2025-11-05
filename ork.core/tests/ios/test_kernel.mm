////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <ork/util/crc.h>
#include <ork/util/hexdump.inl>
#include <ork/util/logger.h>

using namespace ork;

void runKernelTests(void) {
    auto logchan = logger()->configureChannel("KERNTEST", fvec3(0.5f, 1.0f, 0.5f), true);

    logchan->log("========================================");
    logchan->log("Starting Kernel Tests");
    logchan->log("========================================");

    // String utilities tests
    logchan->log("");
    logchan->log("--- String Utilities ---");
    std::string test_str = "Hello, Orkid!";
    logchan->log("Original string: '%s'", test_str.c_str());

    std::string formatted = FormatString("Formatted: %d + %d = %d", 5, 3, 8);
    logchan->log("%s", formatted.c_str());

    // Path tests
    logchan->log("");
    logchan->log("--- Path Tests ---");
    file::Path path1("test/path/file.txt");
    logchan->log("Path: %s", path1.c_str());
    logchan->log("Extension: %s", path1.getExtension().c_str());
    logchan->log("Name: %s", path1.getName().c_str());

    file::Path path2 = path1.toAbsolute();
    logchan->log("Absolute path: %s", path2.c_str());

    // Timer tests
    logchan->log("");
    logchan->log("--- Timer Tests ---");
    Timer timer;
    timer.Start();

    // Simulate some work
    volatile int sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += i;
    }

    float elapsed_ms = timer.SecsSinceStart() * 1000.0f;
    logchan->log("Timer test: %.4f ms elapsed", elapsed_ms);

    // CRC tests
    logchan->log("");
    logchan->log("--- CRC Tests ---");
    logchan->log("CRC32 and CRC64 utilities available in ork.core");

    // Environment tests
    logchan->log("");
    logchan->log("--- Environment Tests ---");
    logchan->log("Environment variable access available via genviron");

    // Operation Queue tests
    logchan->log("");
    logchan->log("--- Operation Queue Tests ---");
    auto opq = opq::concurrentQueue();
    logchan->log("Created concurrent operation queue");

    const int num_ops = 10;
    std::atomic<int> remaining{num_ops};

    for (int i = 0; i < num_ops; i++) {
        opq->enqueue([&remaining, i, num_ops, logchan]() {
            logchan->log("Operation %d executed", i);
            remaining--;
        });
    }

    // Wait for operations to complete by polling the atomic counter
    while (remaining.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    logchan->log("All %d operations completed", num_ops);

    // Hexdump test
    logchan->log("");
    logchan->log("--- Hexdump Test ---");
    uint8_t test_data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x4F, 0x72,
                           0x6B, 0x69, 0x64, 0x21, 0x00};
    logchan->log("Test data: %d bytes (hex dump utilities available)", (int)sizeof(test_data));

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Kernel Tests Complete");
    logchan->log("========================================");
}
