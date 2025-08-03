///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2025, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/ipcq.h>
#include <ork/kernel/timer.h>
#include <errno.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// IpcMsgQSender Implementation
///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::IpcMsgQSender(const std::string& name)
    : _name(name)
    , _outbox(nullptr)
    , _sharedmem(nullptr) {
  // Create log channel for this sender - light blue color
  std::string channel_name = "ipcqS." + name;
  _logchannel = logger()->configureChannel(channel_name, fvec3(0.5, 0.7, 1.0), true);
  
  // Initialize connection
  _initialize();
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::~IpcMsgQSender() {
  if (_sharedmem) {
    _logchannel->log("destroying");
    setSenderState(MessageQueueState::TERMINATED);
    // ShmObject wrapper handles cleanup automatically
  }
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::_initialize() {
  _logchannel->log("initializing msgQ<%s>", _name.c_str());

  // Use open_or_create - will work whether we're first or second
  _sharedmem = ShmObject<msq_impl_t>::realize(_name);
  OrkAssert(_sharedmem != nullptr);
  _outbox = _sharedmem->image();
  
  // Mark sender as running
  setSenderState(MessageQueueState::RUNNING);
  
  _logchannel->log("initialized, will sync on first send");
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::_waitForReceiver() {
  // Wait until receiver is running
  while (getRecieverState() != MessageQueueState::RUNNING) {
    usleep(1 << 16); // ~65ms
  }
  _logchannel->log("receiver is ready");
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::send(const message_t& inc_msg) {
  // Ensure receiver is ready on first send
  static bool first_send = true;
  if (first_send) {
    _waitForReceiver();
    first_send = false;
    _logchannel->log("ready to send");
  }
  
  OrkAssert(_outbox != nullptr);
  OrkAssert(getRecieverState() != MessageQueueState::TERMINATED);
  _outbox->_primaryQ.push(inc_msg);
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::sendDebug(const message_t& inc_msg) {
  OrkAssert(_outbox != nullptr);
  OrkAssert(getRecieverState() != MessageQueueState::TERMINATED);
  _outbox->_debugQ.push(inc_msg);
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::setSenderState(MessageQueueState est) {
  OrkAssert(_outbox != nullptr);
  _outbox->_senderState.store(est);
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
MessageQueueState IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::getRecieverState() const {
  OrkAssert(_outbox != nullptr);
  return _outbox->_recieverState.load();
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::benchSendPerformance(size_t count) {
  // send count messages
  // measure time
  // print bandwidth

  size_t num_sent          = 0;
  size_t index             = 0;
  const size_t num_to_send = count;

  message_t msg;
  for (int i = 0; i < 16; i++) {
    msg.writeString("benchmark");
  }
  size_t msglen = msg.length();
  while (num_sent < num_to_send) {
    send(msg);
    num_sent += msglen;
    index++;
  }
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::sendDataBlock(datablock_ptr_t dblock) {
  if (_profiling_enabled) {
    _profile_timer.Start();
  }
  
  message_t msg;
  msg.writeString("datablock");
  msg.writeString("length");
  msg.template write<size_t>(dblock->length());
  send(msg);
  size_t index         = 0;
  size_t num_remaining = dblock->length();
  const uint8_t* data  = (const uint8_t*)dblock->data();
  while (num_remaining > 0) {
    msg.clear();
    // Account for the actual available space in the message packet
    // MESSAGE_SIZE is the total packet size, but we need to leave room for packet overhead
    size_t available_space = msg.remaining();
    size_t num_this_iter = std::min(num_remaining, available_space);
    msg.writeDataInternal(data + index, num_this_iter);
    send(msg);
    num_remaining -= num_this_iter;
    index += num_this_iter;
  }
  msg.clear();
  msg.writeString("datablock-done");
  send(msg);
  
  if (_profiling_enabled) {
    _profile_timer.End();
    _transfer_time = _profile_timer.SpanInSecs();
    _transfer_size = dblock->length();
  }
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQSender<MESSAGE_SIZE, QUEUE_SIZE>::close() {
  // Set close request flag in shared memory
  _outbox->_closeRequested.store(true);
  
  // Wait for receiver to acknowledge
  while (!_outbox->_closeAcknowledged.load()) {
    usleep(1000); // 1ms
  }
}

///////////////////////////////////////////////////////////////////////////////
// IpcMsgQReciever Implementation
///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::IpcMsgQReciever(const std::string& name)
    : _inbox(nullptr)
    , _sharedmem(nullptr)
    , _name(name) {
  // Create log channel for this receiver - light green color
  std::string channel_name = "ipcqR." + name;
  _logchannel = logger()->configureChannel(channel_name, fvec3(0.5, 1.0, 0.7), true);
  _futureCounter = 0;
  
  // Initialize connection
  _initialize();
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::~IpcMsgQReciever() {
  if (_sharedmem) {
    _logchannel->log("destroying");
    setRecieverState(MessageQueueState::TERMINATED);
    // ShmObject wrapper handles cleanup automatically
  }
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
rpcfuture_ptr_t IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::allocFuture() {
  rpcfuture_ptr_t rval = nullptr;
  bool ok              = _futurePool.try_pop(rval);
  if (ok) {
    OrkAssert(rval != nullptr);
  } else // make a new one
    rval = std::make_shared<ork::RpcFuture>();

  rval->clear();
  rval->setId(_futureCounter++);
  _futureMap.LockForWrite()[rval->getId()] = rval;
  _futureMap.UnLock();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
rpcfuture_ptr_t IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::findFuture(fut_id_t fid) const {
  rpcfuture_ptr_t rval            = nullptr;
  const future_map_t& fmap        = _futureMap.LockForRead();
  typename future_map_t::const_iterator it = fmap.find(fid);
  rval                            = (it == fmap.end()) ? nullptr : it->second;
  _futureMap.UnLock();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::returnFuture(rpcfuture_ptr_t pf) {
  future_map_t& fmap        = _futureMap.LockForWrite();
  typename future_map_t::iterator it = fmap.find(pf->getId());
  OrkAssert(it != fmap.end());
  fmap.erase(it);
  _futureMap.UnLock();
  _futurePool.push(pf);
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::_initialize() {
  _logchannel->log("initializing msgQ<%s>", _name.c_str());

  // Use open_or_create - will work whether we're first or second
  _sharedmem = ShmObject<msq_impl_t>::realize(_name);
  OrkAssert(_sharedmem != nullptr);
  _inbox = _sharedmem->image();
  
  // Mark receiver as running
  setRecieverState(MessageQueueState::RUNNING);
  
  _logchannel->log("initialized, will sync on first receive");
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::_waitForSender() {
  // Wait until sender is running
  while (getSenderState() != MessageQueueState::RUNNING) {
    usleep(1 << 16); // ~65ms  
  }
  _logchannel->log("sender is ready");
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::setRecieverState(MessageQueueState est) {
  OrkAssert(_inbox != nullptr);
  _inbox->_recieverState.store(est);
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
MessageQueueState IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::getSenderState() const {
  OrkAssert(_inbox != nullptr);
  return _inbox->_senderState.load();
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
bool IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::tryReceive(message_t& out_msg) {
  // Ensure sender is ready on first receive
  static bool first_receive = true;
  if (first_receive) {
    _waitForSender();
    first_receive = false;
    _logchannel->log("ready to receive");
  }
  
  OrkAssert(_inbox != nullptr);
  bool bpopped = _inbox->_primaryQ.try_pop(out_msg);
  return bpopped;
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
bool IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::tryReceiveDebug(message_t& out_msg) {
  OrkAssert(_inbox != nullptr);
  bool bpopped = _inbox->_debugQ.try_pop(out_msg);
  return bpopped;
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::benchReceivePerformance(size_t count) {
  // receive count messages
  // measure time
  // print bandwidth

  size_t index             = 0;
  size_t num_recvd         = 0;
  size_t prv_num_recvd     = 0;
  const size_t num_to_recv = count;

  Timer t;
  t.Start();
  while (num_recvd < num_to_recv) {

    message_t msg;
    if (tryReceive(msg)) {
      size_t msglen = msg.length();
      prv_num_recvd = num_recvd;
      num_recvd += msglen;
      index++;
    } else {
      sched_yield();
    }
  }
  double elapsed       = t.SecsSinceStart();
  double bandwidth     = double(num_to_recv) / elapsed;
  double bandwidth_mib = bandwidth / (1024.0 * 1024.0);
  double msgpersec     = double(index) / elapsed;
  double msgpersec_m   = msgpersec / 1e6;
  _logchannel->log("received %zu bytes in %f seconds", num_recvd, elapsed);
  _logchannel->log("bandwidth: %f MiB/sec", bandwidth_mib);
  _logchannel->log("million-msg/sec: %f", msgpersec_m);
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
datablock_ptr_t IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::tryReceiveDataBlock() {
  datablock_ptr_t rval;
  message_t msg;
  std::vector<uint8_t> buffer;
  if (tryReceive(msg)) {
    if (_profiling_enabled) {
      _profile_timer.Start();
    }
    
    message_iterator_t iter(msg);
    std::string msgtype = msg.readString(iter);
    OrkAssert(msgtype == "datablock");
    std::string key = msg.readString(iter);
    if (key == "length") {
      size_t length;
      msg.template read<size_t>(length, iter);
      rval                 = std::make_shared<DataBlock>();
      size_t index         = 0;
      size_t num_remaining = length;
      while (num_remaining > 0) {
        message_t msg;
        if (tryReceive(msg)) {
          // Read the actual amount of data in this message
          // The sender writes only what fits in msg.remaining()
          message_iterator_t iter(msg);
          size_t data_in_msg = msg.length();  // How much data was written to this message
          size_t num_this_iter = std::min(num_remaining, data_in_msg);
          buffer.resize(num_this_iter);
          msg.readDataInternal(buffer.data(), num_this_iter, iter);
          // copy into datablock
          rval->addData(buffer.data(), num_this_iter);
          num_remaining -= num_this_iter;
          index += num_this_iter;
        } else {
          // Don't break - we're expecting more data fragments
          // Just yield and retry
          sched_yield();
        }
      }
      OrkAssert(num_remaining == 0);
      bool done = false;
      while (not done) {
        message_t msg;
        if (tryReceive(msg)) {
          message_iterator_t iter(msg);
          std::string msgtype = msg.readString(iter);
          if (msgtype == "datablock-done") {
            done = true;
          }
        }
      }
      
      if (_profiling_enabled) {
        _profile_timer.End();
        _transfer_time = _profile_timer.SpanInSecs();
        _transfer_size = length;
      }
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
datablock_ptr_t IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::receiveDataBlock() {
  datablock_ptr_t rval;
  while (not rval) {
    rval = tryReceiveDataBlock();
    if (not rval) {
      sched_yield();
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
bool IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::isCloseRequested() const {
  return _inbox->_closeRequested.load();
}

///////////////////////////////////////////////////////////////////////////////

template<size_t MESSAGE_SIZE, size_t QUEUE_SIZE>
void IpcMsgQReciever<MESSAGE_SIZE, QUEUE_SIZE>::acknowledgeClose() {
  _inbox->_closeAcknowledged.store(true);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork