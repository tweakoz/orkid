////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/taskgraph.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace ork;

////////////////////////////////////////////////////////////////////////////////
// Test Cases
////////////////////////////////////////////////////////////////////////////////

TEST(TaskGraphBasicConstruction) {

  bool task1_executed = false;
  bool task2_executed = false;

  auto executor = TaskExecutor::createOPQParallel();
  auto graph = TaskGraph::create();
  auto phase = TaskGraph::phase(graph, "test_phase", executor);
  phase->task("task1", [&](taskgraph_ptr_t g) {
    task1_executed = true;
  });
  phase->task("task2", [&](taskgraph_ptr_t g) {
    task2_executed = true;
  });
  TaskGraph::execute(graph);

  CHECK_EQUAL(1, graph->_phases.size());
  CHECK_EQUAL(2, graph->_phases[0]->_tasks.size());
  CHECK_EQUAL("test_phase", graph->_phases[0]->_name);
}

////////////////////////////////////////////////////////////////////////////////

TEST(TaskGraphExecutionCompletion) {
  auto graph = std::make_shared<TaskGraph>();
  auto executor = TaskExecutor::createOPQParallel();
  
  std::atomic<bool> graph_completed{false};
  std::atomic<int> tasks_executed{0};
  
  auto phase = TaskGraph::phase(graph, "test_phase", executor);
  phase->task("task1", [&](taskgraph_ptr_t g) {
    tasks_executed++;
  });
  phase->task("task2", [&](taskgraph_ptr_t g) {
    tasks_executed++;
  });
  
  TaskGraph::execute(graph, [&](taskgraph_ptr_t g) {
    graph_completed = true;
  });
  
  // Wait for completion
  auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!graph_completed && std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  CHECK(graph_completed.load());
  CHECK_EQUAL(2, tasks_executed.load());
}

////////////////////////////////////////////////////////////////////////////////

TEST(TaskGraphMultiplePhases) {
  auto graph = std::make_shared<TaskGraph>();
  auto parallel_executor = TaskExecutor::createOPQParallel();
  auto serial_executor = TaskExecutor::createSerial();
  
  std::atomic<bool> graph_completed{false};
  std::atomic<int> phase1_tasks{0};
  std::atomic<int> phase2_tasks{0};
  
  auto phase1 = TaskGraph::phase(graph, "phase1", parallel_executor);
  phase1->task("task1", [&](taskgraph_ptr_t g) {
    phase1_tasks++;
  });
  phase1->task("task2", [&](taskgraph_ptr_t g) {
    phase1_tasks++;
  });
       
  auto phase2 = TaskGraph::phase(graph, "phase2", serial_executor);
  phase2->task("task3", [&](taskgraph_ptr_t g) {
    phase2_tasks++;
  });
  phase2->task("task4", [&](taskgraph_ptr_t g) {
    phase2_tasks++;
  });
  
  TaskGraph::execute(graph, [&](taskgraph_ptr_t g) {
    graph_completed = true;
  });
  
  // Wait for completion
  auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!graph_completed && std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  CHECK(graph_completed.load());
  CHECK_EQUAL(2, phase1_tasks.load());
  CHECK_EQUAL(2, phase2_tasks.load());
  CHECK_EQUAL(2, graph->_phases.size());
}

////////////////////////////////////////////////////////////////////////////////

TEST(TaskGraphVarMapDataPassing) {
  auto graph = std::make_shared<TaskGraph>();
  auto executor = TaskExecutor::createOPQParallel();
  
  std::atomic<bool> graph_completed{false};
  std::atomic<int> final_value{0};
  
  auto producer_phase = TaskGraph::phase(graph, "producer", executor);
  producer_phase->task("produce_data", [&](taskgraph_ptr_t g) {
    g->_varmap.atomicOp([](varmap::VarMap& vmap) {
      vmap.set<int>("test_value", 42);
    });
  });
       
  auto consumer_phase = TaskGraph::phase(graph, "consumer", executor);
  consumer_phase->task("consume_data", [&](taskgraph_ptr_t g) {
    g->_varmap.atomicOp([&final_value](varmap::VarMap& vmap) {
      if (auto as_int = vmap.typedValueForKey<int>("test_value")) {
        final_value = as_int.value();
      }
    });
  });
  
  TaskGraph::execute(graph, [&](taskgraph_ptr_t g) {
    graph_completed = true;
  });
  
  // Wait for completion
  auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!graph_completed && std::chrono::steady_clock::now() < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  CHECK(graph_completed.load());
  CHECK_EQUAL(42, final_value.load());
}

////////////////////////////////////////////////////////////////////////////////

TEST(TaskGraphEmptyCompletion) {
  auto graph = std::make_shared<TaskGraph>();
  
  std::atomic<bool> graph_completed{false};
  
  TaskGraph::execute(graph, [&](taskgraph_ptr_t g) {
    graph_completed = true;
  });
  
  CHECK(graph_completed.load());
}

////////////////////////////////////////////////////////////////////////////////

TEST(TaskGraphParallelComputation) {
  auto graph = std::make_shared<TaskGraph>();
  auto parallel_executor = TaskExecutor::createOPQParallel();
  auto serial_executor = TaskExecutor::createSerial();
  
  std::atomic<bool> graph_completed{false};
  
  // We'll compute the sum of squares for a large range in parallel
  // Split into 8 tasks, each computing partial sums
  const int64_t RANGE_START = 1;
  const int64_t RANGE_END = 1000000000; // 1 billion
  const int NUM_WORKERS = 8;
  const int64_t CHUNK_SIZE = (RANGE_END - RANGE_START + 1) / NUM_WORKERS;
  
  // Expected result: sum of i^2 from 1 to N = N*(N+1)*(2N+1)/6
  // For N=1,000,000,000: 333333333833333333500000000
  
  auto compute_phase = TaskGraph::phase(graph, "parallel_compute", parallel_executor);
  
  // Create worker tasks that compute partial sums
  for (int worker_id = 0; worker_id < NUM_WORKERS; worker_id++) {
    std::string task_name = "compute_worker_" + std::to_string(worker_id);
    compute_phase->task(task_name, [worker_id, CHUNK_SIZE, RANGE_START, RANGE_END](taskgraph_ptr_t g) {
      int64_t start = RANGE_START + worker_id * CHUNK_SIZE;
      int64_t end = (worker_id == NUM_WORKERS - 1) ? RANGE_END : start + CHUNK_SIZE - 1;
      
      // Compute sum of squares for this chunk
      __int128_t partial_sum = 0;
      for (int64_t i = start; i <= end; i++) {
        partial_sum += static_cast<__int128_t>(i) * i;
      }
      
      // Store partial result in varmap
      std::string key = "partial_sum_" + std::to_string(worker_id);
      g->_varmap.atomicOp([key, partial_sum](varmap::VarMap& vmap) {
        vmap.set<__int128_t>(key, partial_sum);
      });
      });
  }
  
  // Serial phase to combine results
  auto combine_phase = TaskGraph::phase(graph, "combine_results", serial_executor);
  combine_phase->task("final_sum", [NUM_WORKERS](taskgraph_ptr_t g) {
    g->_varmap.atomicOp([NUM_WORKERS](varmap::VarMap& vmap) {
      __int128_t total_sum = 0;
      
      // Gather all partial sums
      for (int worker_id = 0; worker_id < NUM_WORKERS; worker_id++) {
        std::string key = "partial_sum_" + std::to_string(worker_id);
        if (auto as_int128 = vmap.typedValueForKey<__int128_t>(key)) {
          total_sum += as_int128.value();
        }
      }
      
      // Store final result
      vmap.set<__int128_t>("final_result", total_sum);
    });
  });
  
  TaskGraph::execute(graph, [&](taskgraph_ptr_t g) {
    graph_completed = true;
  });
  
  CHECK(graph_completed.load());
  
  // Verify the result
  __int128_t computed = 0;
  graph->_varmap.atomicOp([&computed](varmap::VarMap& vmap) {
    if (auto as_result = vmap.typedValueForKey<__int128_t>("final_result")) {
      computed = as_result.value();
    }
  });
  
  if (computed != 0) {
    
    // Calculate expected value using formula: n*(n+1)*(2n+1)/6
    // For n=1,000,000,000:
    // n*(n+1)*(2n+1)/6 = 1000000000 * 1000000001 * 2000000001 / 6
    // We need to use __int128_t to avoid overflow
    __int128_t n = RANGE_END;
    __int128_t expected = n * (n + 1) * (2 * n + 1) / 6;
    
    // Can't use CHECK_EQUAL directly with __int128_t, so compare manually
    CHECK(expected == computed);
    
    // Also verify it's the known correct value
    // 333333333833333333500000000 in decimal
    std::string expected_str = "333333333833333333500000000";
    
    // Convert __int128_t to string for comparison
    char buffer[64];
    __int128_t temp = computed;
    int idx = 0;
    if (temp == 0) {
      buffer[idx++] = '0';
    } else {
      char temp_buffer[64];
      int temp_idx = 0;
      while (temp > 0) {
        temp_buffer[temp_idx++] = '0' + (temp % 10);
        temp /= 10;
      }
      // Reverse the string
      while (temp_idx > 0) {
        buffer[idx++] = temp_buffer[--temp_idx];
      }
    }
    buffer[idx] = '\0';
    std::string computed_str(buffer);
    
    CHECK_EQUAL(expected_str, computed_str);
  } else {
    CHECK(false); // Should have a result
  }
}

////////////////////////////////////////////////////////////////////////////////