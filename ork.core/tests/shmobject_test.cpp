////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/util/shmobject.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////
// Test data structures
///////////////////////////////////////////////////////////////////////////////

struct TestData {
  int value;
  float array[4];
  char name[32];
  
  void initializeShmImage() {
    value = 0;
    for(int i = 0; i < 4; i++) array[i] = 0.0f;
    memset(name, 0, sizeof(name));
  }
  
  void uninitializeShmImage() {
    // No special cleanup needed
  }
};

struct CounterData {
  std::atomic<int> counter;
  std::atomic<bool> ready;
  
  void initializeShmImage() {
    counter.store(0);
    ready.store(false);
  }
  
  void uninitializeShmImage() {
    // No special cleanup needed for atomics
  }
};

///////////////////////////////////////////////////////////////////////////////
// Test 1: Basic shared memory create/attach
///////////////////////////////////////////////////////////////////////////////
TEST(shmobject_basic_create_attach) {
  const char* test_name = "test_shmobject_basic";
  
  // Create shared memory (first realize creates it)
  auto shm_create = ShmObject<TestData>::realize(test_name);
  CHECK(shm_create != nullptr);
  CHECK(shm_create->is_creator());
  CHECK(shm_create->size() == sizeof(TestData));
  
  // Write data
  auto* data = shm_create->image();
  data->value = 42;
  data->array[0] = 1.0f;
  data->array[1] = 2.0f;
  data->array[2] = 3.0f;
  data->array[3] = 4.0f;
  strcpy(data->name, "Hello SharedMem");
  
  // Attach to shared memory while creator still exists (second realize attaches)
  auto shm_attach = ShmObject<TestData>::realize(test_name);
  CHECK(shm_attach != nullptr);
  CHECK(!shm_attach->is_creator());
  
  // Read data
  auto* data2 = shm_attach->image();
  CHECK(data2->value == 42);
  CHECK(data2->array[0] == 1.0f);
  CHECK(data2->array[1] == 2.0f);
  CHECK(data2->array[2] == 3.0f);
  CHECK(data2->array[3] == 4.0f);
  CHECK(strcmp(data2->name, "Hello SharedMem") == 0);
  
  // Note: data and data2 are different virtual addresses that map to the same physical memory
  // We can't compare pointers directly, but we can verify they see the same changes
  
  // Modify through first pointer
  data->value = 999;
  
  // Verify change is visible through second pointer
  CHECK(data2->value == 999);
}

///////////////////////////////////////////////////////////////////////////////
// Test 2: Reference counted shared memory
///////////////////////////////////////////////////////////////////////////////
TEST(shmobject_refcounted) {
  const char* test_name = "test_shmobject_refcount";
  
  // Create with ref count
  auto shm1 = RefCountedShmObject<CounterData>::create(test_name);
  CHECK(shm1 != nullptr);
  CHECK(shm1->ref_count() == 1);
  
  // Initialize data
  shm1->image()->counter = 100;
  shm1->image()->ready = false;
  
  // Attach increases ref count
  auto shm2 = RefCountedShmObject<CounterData>::attach(test_name);
  CHECK(shm2 != nullptr);
  CHECK(shm2->ref_count() == 2);
  
  // Verify data is shared
  CHECK(shm2->image()->counter == 100);
  
  // Modify through second reference
  shm2->image()->counter = 200;
  CHECK(shm1->image()->counter == 200);
  
  // Release one reference
  shm2.reset();
  usleep(1000); // Let destructor run
  CHECK(shm1->ref_count() == 1);
  
  // Last reference will clean up
  shm1.reset();
}

///////////////////////////////////////////////////////////////////////////////
// Test 3: Inter-thread synchronization
///////////////////////////////////////////////////////////////////////////////
TEST(shmobject_thread_sync) {
  const char* test_name = "test_shmobject_sync";
  
  auto shm = RefCountedShmObject<CounterData>::create(test_name);
  shm->image()->counter = 0;
  shm->image()->ready = false;
  
  // Producer thread
  auto producer = std::make_shared<Thread>("producer");
  producer->start([test_name](anyp data) {
    auto shm_prod = RefCountedShmObject<CounterData>::attach(test_name);
    
    // Lock and update
    {
      auto lock = shm_prod->lock();
      shm_prod->image()->counter = 42;
      shm_prod->image()->ready = true;
    }
    
    // Notify consumer
    shm_prod->notify_all();
  });
  
  // Consumer waits for ready
  {
    auto lock = shm->lock();
    while (!shm->image()->ready) {
      shm->wait(lock);
    }
  }
  
  // Check result
  CHECK(shm->image()->counter == 42);
  CHECK(shm->image()->ready == true);
  
  producer->join();
}

///////////////////////////////////////////////////////////////////////////////
// Test 4: Inter-process communication (fork)
///////////////////////////////////////////////////////////////////////////////
TEST(shmobject_interprocess_fork) {
  const char* test_name = "test_shmobject_fork";
  
  // Clean up any existing shared memory
  boost::interprocess::shared_memory_object::remove(test_name);
  
  pid_t pid = fork();
  
  if (pid == 0) {
    // Child process - consumer
    usleep(100000); // Let parent create the shared memory
    
    try {
      auto shm = RefCountedShmObject<CounterData>::attach(test_name);
      
      // Wait for data to be ready
      {
        auto lock = shm->lock();
        while (!shm->image()->ready) {
          shm->wait(lock);
        }
      }
      
      // Verify and modify
      if (shm->image()->counter == 100) {
        shm->image()->counter = 200;
        _exit(0); // Success
      } else {
        _exit(1); // Wrong value
      }
    } catch (...) {
      _exit(2); // Exception
    }
  } else {
    // Parent process - producer
    auto shm = RefCountedShmObject<CounterData>::create(test_name);
    shm->image()->counter = 100;
    shm->image()->ready = false;
    
    // Give child time to attach
    usleep(200000);
    
    // Signal data ready
    {
      auto lock = shm->lock();
      shm->image()->ready = true;
    }
    shm->notify_all();
    
    // Wait for child
    int status;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    
    // Verify child modified the data
    CHECK(shm->image()->counter == 200);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Test 5: Operator overloads
///////////////////////////////////////////////////////////////////////////////
TEST(shmobject_operators) {
  const char* test_name = "test_shmobject_ops";
  
  auto shm = ShmObject<TestData>::realize(test_name);
  
  // Test data() method
  shm->image()->value = 123;
  CHECK(shm->image()->value == 123);
  
  // Test * operator (need double dereference: once for shared_ptr, once for ShmObject)
  (**shm).value = 456;
  CHECK((**shm).value == 456);
  
  // Test -> operator on ShmObject
  (*shm)->value = 789;
  CHECK((*shm)->value == 789);
  
  // Test const versions
  const auto& const_shm = shm;
  CHECK(const_shm->image()->value == 789);
  CHECK((**const_shm).value == 789);
}