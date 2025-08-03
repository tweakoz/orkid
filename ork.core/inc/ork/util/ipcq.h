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
#include <ork/kernel/timer.h>
#include <ork/util/shmobject.h>
#include <ork/util/ipcq_packet.h>
#include <ork/util/logger.h>

namespace ork {

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
struct IpcMsgQReciever;

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
struct IpcMsgQSender;

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

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
struct IpcqShmObjectImage {
  using message_t = IpcMessagePacket<MESSAGE_SIZE>;
  static constexpr size_t DEBUG_QUEUE_SIZE = QUEUE_SIZE / 8; // Debug queue is 1/8th of primary
  
  ork::MpMcBoundedQueue<message_t, QUEUE_SIZE> _primaryQ;
  ork::MpMcBoundedQueue<message_t, DEBUG_QUEUE_SIZE> _debugQ;
  ork::atomic<MessageQueueState> _senderState;
  ork::atomic<MessageQueueState> _recieverState;
  
  // Close synchronization
  ork::atomic<bool> _closeRequested;    // Sender sets this when close() is called
  ork::atomic<bool> _closeAcknowledged; // Receiver sets this when it sees close request

  void initializeShmImage() {
    _senderState   = MessageQueueState::INIT;
    _recieverState = MessageQueueState::INIT;
    _closeRequested = false;
    _closeAcknowledged = false;
  }
  
  void uninitializeShmImage() {
    // No special cleanup needed for atomics
  }
};

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
struct IpcMsgQSender {
  using message_t = IpcMessagePacket<MESSAGE_SIZE>;
  using message_iterator_t = IpcMessagePacketIterator<MESSAGE_SIZE>;
  using msq_impl_t = IpcqShmObjectImage<MESSAGE_SIZE, QUEUE_SIZE>;
  using sharedmem_ptr_t = std::shared_ptr<ShmObject<msq_impl_t>>;
  
  IpcMsgQSender(const std::string& name);
  ~IpcMsgQSender();

  void send(const message_t& msg);
  void sendDataBlock(datablock_ptr_t dblock);
  void sendDebug(const message_t& msg);
  void benchSendPerformance(size_t count);
  void close();  // Send graceful shutdown signal and wait for receiver acknowledgment
  MessageQueueState getRecieverState() const;
  
  // Profiling members
  bool _profiling_enabled = false;
  Timer _profile_timer;
  float _transfer_time = 0.0f;
  size_t _transfer_size = 0;

private:
  void _initialize();
  void _waitForReceiver();
  void setSenderState(MessageQueueState est);

  std::string _name;
  logchannel_ptr_t _logchannel;

  msq_impl_t* _outbox;
  sharedmem_ptr_t _sharedmem;
};

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
struct IpcMsgQReciever {
  using message_t = IpcMessagePacket<MESSAGE_SIZE>;
  using message_iterator_t = IpcMessagePacketIterator<MESSAGE_SIZE>;
  using msq_impl_t = IpcqShmObjectImage<MESSAGE_SIZE, QUEUE_SIZE>;
  using sharedmem_ptr_t = std::shared_ptr<ShmObject<msq_impl_t>>;
  
  using fut_id_t     = RpcFuture::rpc_future_id_t;
  using future_map_t = std::map<fut_id_t, rpcfuture_ptr_t>;

  IpcMsgQReciever(const std::string& name);
  ~IpcMsgQReciever();

  bool tryReceive(message_t& msg_out);
  bool tryReceiveDebug(message_t& msg_out);
  datablock_ptr_t tryReceiveDataBlock();
  datablock_ptr_t receiveDataBlock();
  void benchReceivePerformance(size_t count);
  bool isCloseRequested() const;    // Check if sender requested close
  void acknowledgeClose();          // Acknowledge close request
  MessageQueueState getSenderState() const;
  
  // Profiling members
  bool _profiling_enabled = false;
  Timer _profile_timer;
  float _transfer_time = 0.0f;
  size_t _transfer_size = 0;

private:
  void _initialize();
  void _waitForSender();
  void setRecieverState(MessageQueueState est);

  //////////////////////////////////////////
  // future based RPC support
  //////////////////////////////////////////

  rpcfuture_ptr_t allocFuture();
  void returnFuture(rpcfuture_ptr_t);
  rpcfuture_ptr_t findFuture(fut_id_t fid) const;

  //////////////////////////////////////////

  msq_impl_t* _inbox;
  sharedmem_ptr_t _sharedmem;

  //////////////////////////////////////////

  std::string _name;
  logchannel_ptr_t _logchannel;
  static constexpr size_t kmaxfutures = 256;
  ork::MpMcBoundedQueue<rpcfuture_ptr_t, kmaxfutures> _futurePool;
  ork::LockedResource<future_map_t> _futureMap;
  ork::atomic<fut_id_t> _futureCounter;
};

///////////////////////////////////////////////////////////////////////////////
// Convenience aliases for common IPCQ configurations
// Naming convention: ipcq_{QUEUE_SIZE}{MESSAGE_SIZE}_[sender|receiver]_t
///////////////////////////////////////////////////////////////////////////////

// 1K queue, 64 byte messages
using ipcq_1K64_sender_t = IpcMsgQSender<64, 1024>;
using ipcq_1K64_receiver_t = IpcMsgQReciever<64, 1024>;

// 4K queue, 256 byte messages
using ipcq_4K256_sender_t = IpcMsgQSender<256, 4096>;
using ipcq_4K256_receiver_t = IpcMsgQReciever<256, 4096>;

// 8K queue, 1K messages
using ipcq_8K1K_sender_t = IpcMsgQSender<1024, 8192>;
using ipcq_8K1K_receiver_t = IpcMsgQReciever<1024, 8192>;

// 4K queue, 4K messages
using ipcq_4K4K_sender_t = IpcMsgQSender<4096, 4096>;
using ipcq_4K4K_receiver_t = IpcMsgQReciever<4096, 4096>;

// 1K queue, 16K messages
using ipcq_1K16K_sender_t = IpcMsgQSender<16384, 1024>;
using ipcq_1K16K_receiver_t = IpcMsgQReciever<16384, 1024>;

// 1K queue, 64K messages
using ipcq_1K64K_sender_t = IpcMsgQSender<65536, 1024>;
using ipcq_1K64K_receiver_t = IpcMsgQReciever<65536, 1024>;

// 1K queue, 256K messages
using ipcq_1K256K_sender_t = IpcMsgQSender<262144, 1024>;
using ipcq_1K256K_receiver_t = IpcMsgQReciever<262144, 1024>;

// 1K queue, 512K messages
using ipcq_1K512K_sender_t = IpcMsgQSender<524288, 1024>;
using ipcq_1K512K_receiver_t = IpcMsgQReciever<524288, 1024>;

// 1K queue, 1M messages
using ipcq_1K1M_sender_t = IpcMsgQSender<1048576, 1024>;
using ipcq_1K1M_receiver_t = IpcMsgQReciever<1048576, 1024>;

// Legacy compatibility (matches old hardcoded sizes)
using ipcq_sender_t = IpcMsgQSender<256, 8192>;
using ipcq_receiver_t = IpcMsgQReciever<256, 8192>;

} // namespace ork