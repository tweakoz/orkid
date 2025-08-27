////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/kerneltypes.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/mutex.h>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <atomic>

namespace ork {

////////////////////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////////////////////

struct TaskGraph;
struct TaskNode;
struct TaskPhase;
struct TaskExecutor;

using taskgraph_rawptr_t = TaskGraph*;
using taskgraph_ptr_t = std::shared_ptr<TaskGraph>;
using taskgraph_wkptr_t = std::weak_ptr<TaskGraph>;
using tasknode_ptr_t = std::shared_ptr<TaskNode>;
using taskphase_ptr_t = std::shared_ptr<TaskPhase>;
using taskexecutor_ptr_t = std::shared_ptr<TaskExecutor>;
using taskfunc_t = std::function<void(taskgraph_ptr_t)>;
using taskgraphcomplete_func_t = std::function<void(taskgraph_ptr_t)>;
using taskphasecomplete_func_t = std::function<void(taskphase_ptr_t)>;

////////////////////////////////////////////////////////////////////////////////
// TaskExecutor - Abstract base class for different execution contexts
////////////////////////////////////////////////////////////////////////////////

struct TaskExecutor {
  virtual ~TaskExecutor() = default;
  virtual void executePhase(taskphase_ptr_t phase) = 0;
  
  // Factory methods for standard executors
  static taskexecutor_ptr_t createOPQParallel();
  static taskexecutor_ptr_t createSerial();
};

////////////////////////////////////////////////////////////////////////////////

struct TaskNode {
  std::string _name;
  taskfunc_t _func;
};
  
////////////////////////////////////////////////////////////////////////////////

struct TaskPhase {

  TaskPhase( taskgraph_wkptr_t graph,                         //
             const std::string& name,                         //
             taskexecutor_ptr_t executor,                     //
             taskphasecomplete_func_t on_completion=nullptr); //

  taskgraph_rawptr_t task(const std::string& name, //
                          taskfunc_t func);        //
  
  taskexecutor_ptr_t _executor;
  std::string _name;
  std::vector<tasknode_ptr_t> _tasks; 
  taskphasecomplete_func_t _on_completion;
  std::vector<std::string> _depends_on_phases;  // Named phase dependencies
  taskgraph_wkptr_t _graph;  // Weak pointer back to parent graph
};

////////////////////////////////////////////////////////////////////////////////
// TaskGraph - async task execution graph with phases and tasks
////////////////////////////////////////////////////////////////////////////////

struct TaskGraph {

  /////////////////////////
  // Graph construction - static factory pattern (no shared_from_this)
  /////////////////////////

  static taskgraph_ptr_t create();

  static taskphase_ptr_t phase( taskgraph_ptr_t self,                            //
                                const std::string& name,                         //
                                taskexecutor_ptr_t executor,                     //
                                taskphasecomplete_func_t on_completion=nullptr); //
                               
  /////////////////////////
  // Execution - can run on any thread including OPQ workers
  // whatever thread invokes execute() is termed the 'primary execution thread' for this graph
  // and will block until completion
  /////////////////////////

  static void execute( taskgraph_ptr_t self,                                     //
                       taskgraphcomplete_func_t on_completion=nullptr);          //

  /////////////////////////

  LockedResource<varmap::VarMap> _varmap;
  std::vector<taskphase_ptr_t> _phases;
  std::atomic<size_t> _phases_pending{0};
  taskgraphcomplete_func_t _on_completion;
  static std::atomic<int> g_taskgraph_perf_counter;
  static std::atomic<int> g_taskgraph_index;
  static std::atomic<int> g_task_perf_counter;
  static std::atomic<int> g_task_index;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork