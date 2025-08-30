////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/taskgraph.h>
#include <ork/kernel/opq.h>
#include <mutex>
#include <condition_variable>
#include <ork/util/logger.h>

namespace ork {

static logchannel_ptr_t logchan_tg = logger()->getChannel("TASKGRAPH");

////////////////////////////////////////////////////////////////////////////////
// OPQParallelExecutor - Executes all tasks in phase concurrently
////////////////////////////////////////////////////////////////////////////////

class OPQParallelExecutor : public TaskExecutor {
public:
  void executePhase(taskphase_ptr_t phase) override { // synchronous
    if (phase->_tasks.empty()) {
      return;
    }
    
    // Use atomic counter and condition variable for synchronous completion
    auto pending_tasks = std::make_shared<std::atomic<size_t>>(phase->_tasks.size());
    auto mutex = std::make_shared<std::mutex>();
    auto cv = std::make_shared<std::condition_variable>();
    // Launch all tasks in parallel
    auto graph = phase->_graph;  // Get shared_ptr from weak_ptr
    for (auto& task : phase->_tasks) {
      opq::concurrentQueue()->enqueue([=]() {
        // Execute the task with completion callback
        task->_func(graph);
        TaskGraph::g_task_index += 1;
        pending_tasks->fetch_sub(1);
        size_t num_tasks = TaskGraph::g_tasks_pending.fetch_sub(1);
        logchan_tg->log("TaskGraph tasks pending: %zu", num_tasks);
      });
    }
    while(pending_tasks->load()) {
      usleep(1000);
    }    
  }
};

////////////////////////////////////////////////////////////////////////////////
// SerialExecutor - Executes tasks in phase sequentially  
////////////////////////////////////////////////////////////////////////////////

class SerialExecutor : public TaskExecutor {
public:
  void executePhase(taskphase_ptr_t phase) override { // synchronous
    if (phase->_tasks.empty()) {
      return;
    }
    auto mutex = std::make_shared<std::mutex>();
    auto cv = std::make_shared<std::condition_variable>();
    auto phase_complete = std::make_shared<std::atomic<bool>>(false);
    auto graph = phase->_graph;  // Get shared_ptr from weak_ptr
    for( auto task : phase->_tasks ) {
      size_t num_tasks = TaskGraph::g_tasks_pending.fetch_sub(1);
      logchan_tg->log("TaskGraph tasks pending: %zu", num_tasks);
      task->_func(graph);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
// Factory functions
////////////////////////////////////////////////////////////////////////////////

taskexecutor_ptr_t TaskExecutor::createOPQParallel() {
  return std::make_shared<OPQParallelExecutor>();
}

taskexecutor_ptr_t TaskExecutor::createSerial() {
  return std::make_shared<SerialExecutor>();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork