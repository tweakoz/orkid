///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2025, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <sys/types.h>
#include <assert.h>
// #include <ork/traits.h>
#include <stdlib.h>
#include <string.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/mutex.h>
#include <ork/kernel/fixedstring.h>
#include <map>
#include <ork/kernel/atomic.h>
#include <ork/kernel/netpacket.inl>
#include <ork/kernel/datablock.h>

namespace ork {

struct IpcMsgQReciever;
struct IpcMsgQSender;

static constexpr size_t kipcq_message_size = 256;

using ipcq_message_t          = StaticMessagePacket<kipcq_message_size>;
using ipcq_message_iterator_t = MessagePacketIterator<kipcq_message_size>;
using uristring_t             = FixedString<kipcq_message_size>;

using ipcq_reciever_ptr_t = std::shared_ptr<IpcMsgQReciever>;
using ipcq_sender_ptr_t   = std::shared_ptr<IpcMsgQSender>;

static constexpr size_t PRIMARY_QUEUE_SIZE = 8192;
static constexpr size_t DEBUG_QUEUE_SIZE   = 1024;

struct RpcFuture : public Future {
  using rpc_future_id_t = uint64_t;
  rpc_future_id_t _ID   = 0;
};

using rpcfuture_ptr_t = std::shared_ptr<RpcFuture>;

///////////////////////////////////////////////////////////////////////////////

enum class MessageQueueState : uint64_t {
  INIT = 0,
  WAIT,
  RUNNING,
  TERMINATED,
};

struct IpcqSharedMemoryImage {
  ork::MpMcBoundedQueue<ipcq_message_t, PRIMARY_QUEUE_SIZE> _primaryQ;
  ork::MpMcBoundedQueue<ipcq_message_t, DEBUG_QUEUE_SIZE> _debugQ;
  ork::atomic<MessageQueueState> _senderState;
  ork::atomic<MessageQueueState> _recieverState;

  IpcqSharedMemoryImage() {
    _senderState   = MessageQueueState::INIT;
    _recieverState = MessageQueueState::INIT;
  }
};
typedef IpcqSharedMemoryImage msq_impl_t;

///////////////////////////////////////////////////////////////////////////////

struct IpcMsgQSender {
  IpcMsgQSender();
  ~IpcMsgQSender();

  void create(const std::string& nam);
  void connect(const std::string& nam);
  void sendSyncStart();
  void send(const ipcq_message_t& msg);
  void sendDataBlock(datablock_ptr_t dblock);
  void sendDebug(const ipcq_message_t& msg);
  void setName(const std::string& nam);
  void setSenderState(MessageQueueState est);
  MessageQueueState getRecieverState() const;
  void benchSendPerformance(size_t count);

  std::string _name;
  std::string _path;

  msq_impl_t* _outbox;
  void* _SHMaddr;
};

///////////////////////////////////////////////////////////////////////////////

struct IpcMsgQReciever {

  using fut_id_t     = RpcFuture::rpc_future_id_t;
  using future_map_t = std::map<fut_id_t, rpcfuture_ptr_t>;

  IpcMsgQReciever();
  ~IpcMsgQReciever();

  void setName(const std::string& nam);

  void create(const std::string& nam);
  void connect(const std::string& nam);
  void waitSyncStart();
  bool tryReceive(ipcq_message_t& msg_out);
  bool tryReceiveDebug(ipcq_message_t& msg_out);
  datablock_ptr_t tryReceiveDataBlock();
  datablock_ptr_t receiveDataBlock();

  void setRecieverState(MessageQueueState est);
  MessageQueueState getSenderState() const;

  void benchReceivePerformance(size_t count);

  //////////////////////////////////////////
  // future based RPC support
  //////////////////////////////////////////

  rpcfuture_ptr_t allocFuture();
  void returnFuture(rpcfuture_ptr_t);
  rpcfuture_ptr_t findFuture(fut_id_t fid) const;

  //////////////////////////////////////////

  msq_impl_t* _inbox;
  void* _SHMaddr;

  //////////////////////////////////////////

  std::string _name;
  std::string _path;
  static constexpr size_t kmaxfutures = 256;
  ork::MpMcBoundedQueue<rpcfuture_ptr_t, kmaxfutures> _futurePool;
  ork::LockedResource<future_map_t> _futureMap;
  ork::atomic<fut_id_t> _futureCounter;
};

} // namespace ork