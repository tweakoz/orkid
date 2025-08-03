///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/ipcq.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

// Helper template to bind IPCQ sender/receiver pairs
template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void bind_ipcq_variant(py::module& module, const char* size_name) {
  using sender_t = IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>;
  using receiver_t = IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>;
  using sender_ptr_t = std::shared_ptr<sender_t>;
  using receiver_ptr_t = std::shared_ptr<receiver_t>;
  
  auto type_codec = python::pb11_typecodec_t::instance();
  
  // Bind sender
  std::string sender_name = std::string(size_name) + "Sender";
  auto sender_type = py::class_<sender_t, sender_ptr_t>(module, sender_name.c_str());
  sender_type.def(py::init<const std::string&>(), py::arg("name"));
  sender_type.def("benchSendPerformance", [](sender_ptr_t sender, size_t count) {
    py::gil_scoped_release release;
    sender->benchSendPerformance(count);
  });
  sender_type.def("sendDataBlock", [](sender_ptr_t sender, datablock_ptr_t db) {
    py::gil_scoped_release release;
    sender->sendDataBlock(db);
  });
  // Add message size and queue size as read-only properties
  sender_type.def_property_readonly_static("message_size", [](py::object) { return MESSAGE_SIZE; });
  sender_type.def_property_readonly_static("queue_size", [](py::object) { return QUEUE_SIZE; });
  // Add profiling properties
  sender_type.def_readwrite("_profiling_enabled", &sender_t::_profiling_enabled);
  sender_type.def_readonly("_transfer_time", &sender_t::_transfer_time);
  sender_type.def_readonly("_transfer_size", &sender_t::_transfer_size);
  type_codec->registerStdCodec<sender_ptr_t>(sender_type);
  
  // Bind receiver
  std::string receiver_name = std::string(size_name) + "Receiver";
  auto receiver_type = py::class_<receiver_t, receiver_ptr_t>(module, receiver_name.c_str());
  receiver_type.def(py::init<const std::string&>(), py::arg("name"));
  receiver_type.def("benchReceivePerformance", [](receiver_ptr_t receiver, size_t count) {
    py::gil_scoped_release release;
    receiver->benchReceivePerformance(count);
  });
  receiver_type.def("tryReceiveDataBlock", [](receiver_ptr_t receiver) -> datablock_ptr_t {
    py::gil_scoped_release release;
    datablock_ptr_t dblock = receiver->tryReceiveDataBlock();
    return dblock;
  });
  receiver_type.def("receiveDataBlock", [](receiver_ptr_t receiver) -> datablock_ptr_t {
    py::gil_scoped_release release;
    datablock_ptr_t dblock = receiver->receiveDataBlock();
    return dblock;
  });
  // Add message size and queue size as read-only properties
  receiver_type.def_property_readonly_static("message_size", [](py::object) { return MESSAGE_SIZE; });
  receiver_type.def_property_readonly_static("queue_size", [](py::object) { return QUEUE_SIZE; });
  // Add profiling properties
  receiver_type.def_readwrite("_profiling_enabled", &receiver_t::_profiling_enabled);
  receiver_type.def_readonly("_transfer_time", &receiver_t::_transfer_time);
  receiver_type.def_readonly("_transfer_size", &receiver_t::_transfer_size);
  type_codec->registerStdCodec<receiver_ptr_t>(receiver_type);
}

void pyinit_ipcq(py::module& module_core) {
  auto dfgmodule = module_core.def_submodule("ipcq", "Interprocess Communications Queue");
  
  // Bind all size variants with standardized naming
  bind_ipcq_variant<64, 1024>(dfgmodule, "Ipcq1K64");       // 1K queue, 64 byte messages
  bind_ipcq_variant<256, 4096>(dfgmodule, "Ipcq4K256");     // 4K queue, 256 byte messages
  bind_ipcq_variant<1024, 8192>(dfgmodule, "Ipcq8K1K");     // 8K queue, 1K messages
  bind_ipcq_variant<4096, 4096>(dfgmodule, "Ipcq4K4K");     // 4K queue, 4K messages
  bind_ipcq_variant<16384, 1024>(dfgmodule, "Ipcq1K16K");   // 1K queue, 16K messages
  bind_ipcq_variant<65536, 1024>(dfgmodule, "Ipcq1K64K");   // 1K queue, 64K messages
  bind_ipcq_variant<262144, 1024>(dfgmodule, "Ipcq1K256K"); // 1K queue, 256K messages
  bind_ipcq_variant<524288, 1024>(dfgmodule, "Ipcq1K512K"); // 1K queue, 512K messages
  bind_ipcq_variant<1048576, 1024>(dfgmodule, "Ipcq1K1M");   // 1K queue, 1M messages
  
  // Legacy compatibility - bind the default size as "Sender" and "Receiver"
  using ipcq_sender_ptr_t = std::shared_ptr<ipcq_sender_t>;
  using ipcq_reciever_ptr_t = std::shared_ptr<ipcq_receiver_t>;
  
  auto type_codec = python::pb11_typecodec_t::instance();
  
  auto sender_type = py::class_<ipcq_sender_t, ipcq_sender_ptr_t>(dfgmodule, "Sender");
  sender_type.def(py::init<const std::string&>(), py::arg("name"));
  sender_type.def("benchSendPerformance", [](ipcq_sender_ptr_t sender, size_t count) {
    py::gil_scoped_release release;
    sender->benchSendPerformance(count);
  });
  sender_type.def("sendDataBlock", [](ipcq_sender_ptr_t sender, datablock_ptr_t db) {
    py::gil_scoped_release release;
    sender->sendDataBlock(db);
  });
  // Add profiling properties
  sender_type.def_readwrite("_profiling_enabled", &ipcq_sender_t::_profiling_enabled);
  sender_type.def_readonly("_transfer_time", &ipcq_sender_t::_transfer_time);
  sender_type.def_readonly("_transfer_size", &ipcq_sender_t::_transfer_size);
  type_codec->registerStdCodec<ipcq_sender_ptr_t>(sender_type);

  auto receiver_type = py::class_<ipcq_receiver_t, ipcq_reciever_ptr_t>(dfgmodule, "Receiver");
  receiver_type.def(py::init<const std::string&>(), py::arg("name"));
  receiver_type.def("benchReceivePerformance", [](ipcq_reciever_ptr_t receiver, size_t count) {
    py::gil_scoped_release release;
    receiver->benchReceivePerformance(count);
  });
  receiver_type.def("tryReceiveDataBlock", [](ipcq_reciever_ptr_t receiver) -> datablock_ptr_t {
    py::gil_scoped_release release;
    datablock_ptr_t dblock = receiver->tryReceiveDataBlock();
    return dblock;
  });
  receiver_type.def("receiveDataBlock", [](ipcq_reciever_ptr_t receiver) -> datablock_ptr_t {
    py::gil_scoped_release release;
    datablock_ptr_t dblock = receiver->receiveDataBlock();
    return dblock;
  });
  // Add profiling properties
  receiver_type.def_readwrite("_profiling_enabled", &ipcq_receiver_t::_profiling_enabled);
  receiver_type.def_readonly("_transfer_time", &ipcq_receiver_t::_transfer_time);
  receiver_type.def_readonly("_transfer_size", &ipcq_receiver_t::_transfer_size);
  type_codec->registerStdCodec<ipcq_reciever_ptr_t>(receiver_type);
  
  // Add a summary function to list available sizes
  dfgmodule.def("list_sizes", []() {
    py::dict sizes;
    sizes["ipcq_1K64"] = py::dict("message_size"_a=64, "queue_size"_a=1024, "description"_a="IPCQ_1K64: 1K queue, 64 byte messages");
    sizes["ipcq_4K256"] = py::dict("message_size"_a=256, "queue_size"_a=4096, "description"_a="IPCQ_4K256: 4K queue, 256 byte messages");
    sizes["ipcq_8K1K"] = py::dict("message_size"_a=1024, "queue_size"_a=8192, "description"_a="IPCQ_8K1K: 8K queue, 1K messages");
    sizes["ipcq_4K4K"] = py::dict("message_size"_a=4096, "queue_size"_a=4096, "description"_a="IPCQ_4K4K: 4K queue, 4K messages");
    sizes["ipcq_1K16K"] = py::dict("message_size"_a=16384, "queue_size"_a=1024, "description"_a="IPCQ_1K16K: 1K queue, 16K messages");
    sizes["ipcq_1K64K"] = py::dict("message_size"_a=65536, "queue_size"_a=1024, "description"_a="IPCQ_1K64K: 1K queue, 64K messages");
    sizes["ipcq_1K256K"] = py::dict("message_size"_a=262144, "queue_size"_a=1024, "description"_a="IPCQ_1K256K: 1K queue, 256K messages");
    sizes["ipcq_1K512K"] = py::dict("message_size"_a=524288, "queue_size"_a=1024, "description"_a="IPCQ_1K512K: 1K queue, 512K messages");
    sizes["ipcq_1K1M"] = py::dict("message_size"_a=1048576, "queue_size"_a=1024, "description"_a="IPCQ_1K1M: 1K queue, 1M messages");
    sizes["legacy"] = py::dict("message_size"_a=256, "queue_size"_a=8192, "description"_a="Legacy: 8K queue, 256 byte messages");
    return sizes;
  });
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork