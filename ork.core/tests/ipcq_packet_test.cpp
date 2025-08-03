////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/util/ipcq_packet.h>
#include <ork/kernel/string/deco.inl>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////
// Test 1: Verify POD properties
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_packet_is_pod) {
    using TestPacket = IpcMessagePacket<1024>;
    
    // These will fail to compile if not POD
    CHECK(std::is_trivially_copyable<TestPacket>::value);
    CHECK(std::is_standard_layout<TestPacket>::value);
    
    // Size check
    CHECK(sizeof(TestPacket) == 1024 + sizeof(size_t) * 2);
    
    printf("IpcMessagePacket<1024> size: %zu bytes\n", sizeof(TestPacket));
    printf("Is trivially copyable: %s\n", std::is_trivially_copyable<TestPacket>::value ? "yes" : "no");
    printf("Is standard layout: %s\n", std::is_standard_layout<TestPacket>::value ? "yes" : "no");
}

///////////////////////////////////////////////////////////////////////////////
// Test 2: Basic read/write operations
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_packet_basic_readwrite) {
    IpcMessagePacket<256> packet;
    
    // Write some data
    packet.write<int>(42);
    packet.write<float>(3.14f);
    packet.writeString("hello world");
    packet.write<uint64_t>(0xDEADBEEF);
    
    // Check length
    CHECK(packet.length() > 0);
    CHECK(packet.length() < 256);
    
    // Reset read position and read back
    packet.resetRead();
    
    int ival = packet.read<int>();
    CHECK(ival == 42);
    
    float fval = packet.read<float>();
    CHECK(fabs(fval - 3.14f) < 0.001f);
    
    std::string sval = packet.readString();
    CHECK(sval == "hello world");
    
    uint64_t uval = packet.read<uint64_t>();
    CHECK(uval == 0xDEADBEEF);
}

///////////////////////////////////////////////////////////////////////////////
// Test 3: Boundary checks
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_packet_boundaries) {
    IpcMessagePacket<64> small_packet;
    
    // Try to write more than capacity
    size_t initial_len = small_packet.length();
    
    // Write until full
    while(small_packet.remaining() >= sizeof(uint32_t)) {
        small_packet.write<uint32_t>(0x12345678);
    }
    
    size_t full_len = small_packet.length();
    
    // Try to write more (should not increase length)
    small_packet.write<uint64_t>(0xDEADBEEF);
    CHECK(small_packet.length() == full_len);
    
    // Verify we can read what we wrote
    small_packet.resetRead();
    int count = 0;
    while(small_packet._readIndex + sizeof(uint32_t) <= small_packet._writeIndex) {
        uint32_t val = small_packet.read<uint32_t>();
        CHECK(val == 0x12345678);
        count++;
    }
    CHECK(count > 0);
}

///////////////////////////////////////////////////////////////////////////////
// Test 4: Raw data operations
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_packet_raw_data) {
    IpcMessagePacket<512> packet;
    
    // Create test data
    uint8_t test_data[128];
    for(int i = 0; i < 128; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Write raw data
    packet.writeData(test_data, 128);
    CHECK(packet.length() == 128);
    
    // Read back
    uint8_t read_data[128];
    packet.resetRead();
    packet.readData(read_data, 128);
    
    // Verify
    for(int i = 0; i < 128; i++) {
        CHECK(read_data[i] == (i & 0xFF));
    }
}

///////////////////////////////////////////////////////////////////////////////
// Test 5: Copy semantics (important for shared memory)
///////////////////////////////////////////////////////////////////////////////
TEST(ipcq_packet_copy) {
    IpcMessagePacket<256> packet1;
    packet1.write<int>(42);
    packet1.writeString("test");
    
    // POD copy
    IpcMessagePacket<256> packet2 = packet1;
    
    // Verify copy
    CHECK(packet2.length() == packet1.length());
    
    packet2.resetRead();
    CHECK(packet2.read<int>() == 42);
    CHECK(packet2.readString() == "test");
    
    // Memcpy test (simulating shared memory transfer)
    IpcMessagePacket<256> packet3;
    std::memcpy(&packet3, &packet1, sizeof(packet1));
    
    packet3.resetRead();
    CHECK(packet3.read<int>() == 42);
    CHECK(packet3.readString() == "test");
}