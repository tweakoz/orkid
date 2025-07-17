////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/opq.h>
#include <ork/kernel/timer.h>

namespace ork {

void pyinit_opq(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // OPQ Performance Data
  /////////////////////////////////////////////////////////////////////////////////
  auto opq_perfdata_type = py::class_<opq::OPQPerfData, opq::opq_perfdata_ptr_t>(module_core, "OPQPerfData")
    .def(py::init<>())
    .def_readwrite("aggregate_ops_per_sec", &opq::OPQPerfData::aggregate_ops_per_sec)
    .def_readwrite("num_threads", &opq::OPQPerfData::num_threads)
    .def_readwrite("avg_latency_ms", &opq::OPQPerfData::avg_latency_ms)
    .def_readwrite("max_latency_ms", &opq::OPQPerfData::max_latency_ms)
    .def_readwrite("pending_ops", &opq::OPQPerfData::pending_ops)
    .def_readwrite("completed_ops", &opq::OPQPerfData::completed_ops)
    .def_readwrite("update_time", &opq::OPQPerfData::update_time)
    .def_readwrite("queue_name", &opq::OPQPerfData::queue_name)
    .def("__repr__", [](opq::opq_perfdata_ptr_t perf) -> std::string {
      return FormatString("OPQPerfData(queue='%s', threads=%d, ops/sec=%.1f, latency=%.1f/%.1fms[max_avg])", 
        perf->queue_name.c_str(), perf->num_threads, perf->aggregate_ops_per_sec, perf->avg_latency_ms, perf->max_latency_ms);
    });
  type_codec->registerStdCodec<opq::opq_perfdata_ptr_t>(opq_perfdata_type);

  // Note: Op class is intentionally not exposed to Python as it's an internal implementation detail
  // Users interact with the queue directly via enqueue() methods

  /////////////////////////////////////////////////////////////////////////////////
  // ConcurrencyGroup
  /////////////////////////////////////////////////////////////////////////////////
  auto concurrency_group_type = py::class_<opq::ConcurrencyGroup, opq::concurrency_group_ptr_t>(module_core, "ConcurrencyGroup")
    .def_property_readonly("name", [](opq::concurrency_group_ptr_t grp) -> std::string { return grp->_name; })
    .def_readwrite("limit_maxops_inflight", &opq::ConcurrencyGroup::_limit_maxops_inflight)
    .def_readwrite("limit_maxops_enqueued", &opq::ConcurrencyGroup::_limit_maxops_enqueued)
    .def_readwrite("limit_maxrunlength", &opq::ConcurrencyGroup::_limit_maxrunlength)
    .def("enqueue", py::overload_cast<const opq::Op&>(&opq::ConcurrencyGroup::enqueue))
    .def("drain", &opq::ConcurrencyGroup::drain)
    .def("makeSerial", &opq::ConcurrencyGroup::MakeSerial)
    .def("__repr__", [](opq::concurrency_group_ptr_t grp) -> std::string {
      return FormatString("ConcurrencyGroup(name='%s', max_inflight=%zu)", 
        grp->_name.c_str(), grp->_limit_maxops_inflight);
    });
  type_codec->registerStdCodec<opq::concurrency_group_ptr_t>(concurrency_group_type);

  /////////////////////////////////////////////////////////////////////////////////
  // OperationsQueue
  /////////////////////////////////////////////////////////////////////////////////
  auto operations_queue_type = py::class_<opq::OperationsQueue, opq::opq_ptr_t>(module_core, "OperationsQueue")
    .def(py::init<int, const char*>(), py::arg("num_threads"), py::arg("name") = "DefOpQ")
    .def_property_readonly("name", [](opq::opq_ptr_t opq) -> std::string { return opq->_name; })
    .def_property_readonly("num_threads_running", [](opq::opq_ptr_t opq) -> int { 
      return opq->_numThreadsRunning.load(); 
    })
    .def_property_readonly("num_pending_operations", [](opq::opq_ptr_t opq) -> int { 
      return opq->_numPendingOperations.load(); 
    })
    .def_property_readonly("num_completed_operations", [](opq::opq_ptr_t opq) -> int { 
      return opq->_numCompletedOperations.load(); 
    })
    .def("enqueue", py::overload_cast<const ork::void_lambda_t&, const std::string&>(&opq::OperationsQueue::enqueue),
         py::arg("operation"), py::arg("name") = "")
    .def("enqueueAndWait", py::overload_cast<const opq::Op&>(&opq::OperationsQueue::enqueueAndWait))
    .def("sync", &opq::OperationsQueue::sync)
    .def("drain", &opq::OperationsQueue::drain)
    .def("createConcurrencyGroup", &opq::OperationsQueue::createConcurrencyGroup,
         py::return_value_policy::reference_internal)
    .def("getPerformanceData", &opq::OperationsQueue::getPerformanceData)
    .def("startPerformanceTracking", &opq::OperationsQueue::startPerformanceTracking,
         "Start performance tracking to measure ops/sec and latency")
    .def("stopPerformanceTracking", &opq::OperationsQueue::stopPerformanceTracking,
         "Stop performance tracking")
    .def("__repr__", [](opq::opq_ptr_t opq) -> std::string {
      return FormatString("OperationsQueue(name='%s', threads=%d, pending=%d, completed=%d)", 
        opq->_name.c_str(), 
        opq->_numThreadsRunning.load(),
        opq->_numPendingOperations.load(),
        opq->_numCompletedOperations.load());
    });
  type_codec->registerStdCodec<opq::opq_ptr_t>(operations_queue_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Module-level functions
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("opq_init", &opq::init);
  module_core.def("opq_updateSerialQueue", &opq::updateSerialQueue, py::return_value_policy::reference);
  module_core.def("opq_mainSerialQueue", &opq::mainSerialQueue, py::return_value_policy::reference);
  module_core.def("opq_concurrentQueue", &opq::concurrentQueue, py::return_value_policy::reference);

  /////////////////////////////////////////////////////////////////////////////////
  // Convenience functions for creating workloads
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("opq_createTestWorkload", [](opq::opq_ptr_t opq, int num_operations, float work_duration) {
    for (int i = 0; i < num_operations; i++) {
      opq->enqueue([i, work_duration]() {
        auto start_time = Timer::get_sync_time();
        // Simulate work by busy waiting
        while (Timer::get_sync_time() - start_time < work_duration) {
          // Busy wait
        }
      }, FormatString("test_op_%d", i));
    }
  }, py::arg("opq"), py::arg("num_operations"), py::arg("work_duration"));

  module_core.def("opq_createBurstWorkload", [](opq::opq_ptr_t opq, int burst_size, int num_bursts, float burst_interval) {
    for (int burst = 0; burst < num_bursts; burst++) {
      for (int i = 0; i < burst_size; i++) {
        opq->enqueue([burst, i]() {
          // Light work
          volatile int x = 0;
          for (int j = 0; j < 1000; j++) {
            x += j;
          }
        }, FormatString("burst_%d_op_%d", burst, i));
      }
      
      // Wait between bursts
      if (burst < num_bursts - 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds((int)(burst_interval * 1000)));
      }
    }
  }, py::arg("opq"), py::arg("burst_size"), py::arg("num_bursts"), py::arg("burst_interval"));
}

} // namespace ork 