////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/taskgraph.h>
#include <ork/kernel/opq.h>
#include <ork/pch.h>
#include <algorithm>
#include <ork/util/logger.h>

namespace ork {

static logchannel_ptr_t logchan_tg = logger()->configureChannel("TASKGRAPH", fvec3(0.4, 0.8, 0.1), true);

////////////////////////////////////////////////////////////////////////////////
// TaskPhase implementation
////////////////////////////////////////////////////////////////////////////////

TaskPhase::TaskPhase(taskgraph_wkptr_t graph,                  //
                     const std::string& name,                  //
                     taskexecutor_ptr_t executor,              //
                     taskphasecomplete_func_t on_completion)   //
  : _executor(executor)
  , _name(name)
  , _on_completion(on_completion)
  , _graph(graph) {

}

////////////////////////////////////////////////////////////////////////////////
// TaskGraph implementation
////////////////////////////////////////////////////////////////////////////////

taskgraph_ptr_t TaskGraph::create() {
  return std::make_shared<TaskGraph>();
}

taskphase_ptr_t TaskGraph::phase(taskgraph_ptr_t self,                     //
                                 const std::string& name,                  //
                                 taskexecutor_ptr_t executor,              //
                                 taskphasecomplete_func_t on_completion) { //
  auto new_phase = std::make_shared<TaskPhase>(self,name,executor,on_completion);
  self->_phases.push_back(new_phase);  
  return new_phase;
}

////////////////////////////////////////////////////////////////////////////////

taskgraph_rawptr_t TaskPhase::task(const std::string& name, taskfunc_t func) {  
  auto new_task = std::make_shared<TaskNode>();
  new_task->_name = name;
  new_task->_func = func;
  _tasks.push_back(new_task);
  return _graph.lock().get();
}

////////////////////////////////////////////////////////////////////////////////
std::atomic<int> TaskGraph::g_taskgraph_perf_counter{0};
std::atomic<int> TaskGraph::g_taskgraph_index{0};
std::atomic<int> TaskGraph::g_task_perf_counter{0};
std::atomic<int> TaskGraph::g_task_index{0};

void TaskGraph::execute(taskgraph_ptr_t self, taskgraphcomplete_func_t on_completion) {
  self->_on_completion = on_completion;

  //////////////////////////////////////
  // debug: print out phase list
  //////////////////////////////////////

  int iphase = 0;
  for (auto phase : self->_phases) {
    auto name = phase->_name;
    int num_tasks = int(phase->_tasks.size());
    logchan_tg->log("TaskGraph: phase %s numtasks<%d>", name.c_str(), num_tasks);
    iphase++;
  }

  //////////////////////////////////////
  // Execute phases sequentially
  //////////////////////////////////////

  for (auto phase : self->_phases) {

    OrkAssert(phase->_executor);

    //////////////////////////////////////
    // The executor should handle phase completion synchronously
    //////////////////////////////////////

    //logchan_tg->log("TaskGraph: begin phase %s", phase->_name.c_str());
    phase->_executor->executePhase(phase);
    //logchan_tg->log("TaskGraph: end phase %s", phase->_name.c_str());

    //////////////////////////////////////
    // Phase is now complete, invoke phase completion callback if any
    //////////////////////////////////////

    if (phase->_on_completion) {
      //logchan_tg->log("TaskGraph: end phase %s invoking on_completion_handler", phase->_name.c_str());
      phase->_on_completion(phase);
    }
  }
  
  //////////////////////////////////////
  // All phases complete, invoke graph completion
  //////////////////////////////////////

  if (on_completion) {
    on_completion(self);
  }

  g_taskgraph_index += 1;
  if((g_taskgraph_index&0xf)==0) {
    logchan_tg->log("TaskGraphs completed: %zu", g_taskgraph_index.load());
  }

}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork