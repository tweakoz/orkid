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
void pyinit_ipcq(py::module& module_core) {
  auto dfgmodule   = module_core.def_submodule("ipcq", "Interprocess Communications Queue");
  auto type_codec  = python::pb11_typecodec_t::instance();
  auto sender_type = py::class_<IpcMsgQSender, ipcq_sender_ptr_t>(dfgmodule, "Sender");
  sender_type.def(py::init<>());
  sender_type.def("create", &IpcMsgQSender::create);
  // sender_type.def("connect", &IpcMsgQSender::Connect );
  sender_type.def("sendSyncStart", &IpcMsgQSender::sendSyncStart);
  sender_type.def("benchSendPerformance", [](ipcq_sender_ptr_t sender, size_t count) {
    py::gil_scoped_release release;
    sender->benchSendPerformance(count);
  });
  sender_type.def("sendDataBlock", [](ipcq_sender_ptr_t sender, datablock_ptr_t db) {
    py::gil_scoped_release release;
    sender->sendDataBlock(db);
  });
  type_codec->registerStdCodec<ipcq_sender_ptr_t>(sender_type);

  auto receiver_type = py::class_<IpcMsgQReciever, ipcq_reciever_ptr_t>(dfgmodule, "Receiver");
  receiver_type.def(py::init<>());
  // receiver_type.def("create", &IpcMsgQReciever::create );
  receiver_type.def("connect", &IpcMsgQReciever::connect);
  receiver_type.def("waitSyncStart", &IpcMsgQReciever::waitSyncStart);
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
  type_codec->registerStdCodec<ipcq_sender_ptr_t>(receiver_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
