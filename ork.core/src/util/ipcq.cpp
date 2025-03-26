///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2025, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////

#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <errno.h>

#include <ork/util/ipcq.h>
#include <ork/file/path.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/timer.h>

static const int kmaxmsgsiz = sizeof(ork::ipcq_message_t);

// static const uin32_t kmapaddrflags = MAP_SHARED|MAP_LOCKED;
static const uint32_t kmapaddrflags = MAP_SHARED;

#define __MSGQ_DEBUG__

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IpcMsgQSender::IpcMsgQSender()
    : _outbox(nullptr)
    , _SHMaddr(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

IpcMsgQSender::~IpcMsgQSender() {
  if (_SHMaddr) {
    printf("destroying IpcMsgQSender<%p>\n", this);
    setSenderState(MessageQueueState::TERMINATED);
    munmap(_SHMaddr, sizeof(msq_impl_t));
    shm_unlink(_path.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::setName(const std::string& nam) {

  ork::uristring_t path;

#if defined(__APPLE__)
  _name = nam;
  path.format("%s", nam.c_str());
#else
  _name = nam;
  path.format("/dev/shm/%s", nam.c_str());
#endif

  _path = path.c_str();
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::create(const std::string& nam) {
  setName(nam);

  constexpr size_t isize = sizeof(msq_impl_t);

#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQSender<%p> creating msgQ named<%s> of size<%zu>\n", this, nam.c_str(), isize);
#endif

  int unlink_ok = shm_unlink(_name.c_str());
  errno         = 0;
  OrkAssert(errno == 0);

  int shm_fd = shm_open(_name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  OrkAssert(shm_fd >= 0);
  OrkAssert(errno == 0);

  int ift = ftruncate(shm_fd, isize);
  OrkAssert(errno == 0);
  _SHMaddr = mmap(0, isize, PROT_READ | PROT_WRITE, kmapaddrflags, shm_fd, 0);
  close(shm_fd);
  OrkAssert(errno == 0);
  _outbox            = (msq_impl_t*)_SHMaddr;
  const char* errstr = "";

  if (errno) {
    errstr = strerror(errno);
  }

#ifdef __MSGQ_DEBUG__
  printf(
      "IpcMsgQSender<%p:%s> shm FD<%d> errno<%s> size<%d>\n", //
      this,                                                   //
      _name.c_str(),                                          //
      shm_fd,                                                 //
      errstr,                                                 //
      int(isize));
#endif

  OrkAssert(_SHMaddr != (void*)0xffffffffffffffff);
  new (_outbox) msq_impl_t;
  sendSyncStart();
  setSenderState(MessageQueueState::RUNNING);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::connect(const std::string& nam) {
  setName(nam);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQSender<%p> connecting to msgQ<%s>\n", this, nam.c_str());
#endif

  while (false == file::Path(_path).isFile()) {
    usleep(1 << 18);
  }

  size_t isize = sizeof(msq_impl_t);
  int shm_id   = shm_open(_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  OrkAssert(shm_id >= 0);
  _SHMaddr = mmap(0, isize, PROT_READ | PROT_WRITE, kmapaddrflags, shm_id, 0);
  _outbox  = (msq_impl_t*)_SHMaddr;
  close(shm_id);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQSender<%p> connected to msgQ<%s>\n", this, nam.c_str());
#endif
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::sendSyncStart() {
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQSender<%p> sending  start/sync\n", this);
#endif
  ork::ipcq_message_t msg;
  msg.writeString("start/sync");
  uint32_t msg_priority = 0;
  _outbox->_primaryQ.push(msg);
  // mOutgoingIpcQ->send(&msg,sizeof(msg),msg_priority);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::send(const ipcq_message_t& inc_msg) {
  OrkAssert(_outbox != nullptr);
  OrkAssert(getRecieverState() != MessageQueueState::TERMINATED);
  _outbox->_primaryQ.push(inc_msg);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::sendDebug(const ipcq_message_t& inc_msg) {
  OrkAssert(_outbox != nullptr);
  OrkAssert(getRecieverState() != MessageQueueState::TERMINATED);
  _outbox->_debugQ.push(inc_msg);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::setSenderState(MessageQueueState est) {
  OrkAssert(_outbox != nullptr);
  OrkAssert(_outbox != nullptr);
  _outbox->_senderState.store(est);
}

///////////////////////////////////////////////////////////////////////////////

MessageQueueState IpcMsgQSender::getRecieverState() const {
  OrkAssert(_outbox != nullptr);
  return _outbox->_recieverState.load();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IpcMsgQReciever::IpcMsgQReciever()
    : _inbox(nullptr)
    , _SHMaddr(nullptr) {
  _futureCounter = 0;
}

///////////////////////////////////////////////////////////////////////////////

IpcMsgQReciever::~IpcMsgQReciever() {
  if (_SHMaddr) {
    printf("destroying IpcMsgQReciever<%p>\n", this);
    setRecieverState(MessageQueueState::TERMINATED);
    munmap(_SHMaddr, sizeof(msq_impl_t));
    shm_unlink(_path.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
// allocate a future
//  if one is not available in the pool, a new one will be heap-allocated
// once allocated, a msgq unique future ID will be assigned
//  and the future registered to that ID for later search
//
// future's are attached/registered to msq recievers
//  because msq-reciever's will have to look a future
//  up when the future's associated message reply
//  is recieved.
///////////////////////////////////////////////////////////////////////////////

rpcfuture_ptr_t IpcMsgQReciever::allocFuture() {
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
// find an allocated future by msgq unique future ID
///////////////////////////////////////////////////////////////////////////////

rpcfuture_ptr_t IpcMsgQReciever::findFuture(fut_id_t fid) const {
  rpcfuture_ptr_t rval            = nullptr;
  const future_map_t& fmap        = _futureMap.LockForRead();
  future_map_t::const_iterator it = fmap.find(fid);
  rval                            = (it == fmap.end()) ? nullptr : it->second;
  _futureMap.UnLock();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// return future to msq's future pool,
//  de-register it's ID
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::returnFuture(rpcfuture_ptr_t pf) {
  future_map_t& fmap        = _futureMap.LockForWrite();
  future_map_t::iterator it = fmap.find(pf->getId());
  OrkAssert(it != fmap.end());
  fmap.erase(it);
  _futureMap.UnLock();
  _futurePool.push(pf);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::setName(const std::string& nam) {
  ork::uristring_t path;

#if defined(__APPLE__)
  _name = nam;
  path.format("%s", nam.c_str());
#else
  _name = nam;
  path.format("/dev/shm/%s", nam.c_str());
#endif

  _path = path.c_str();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::connect(const std::string& nam) {
  setName(nam);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQReciever<%p> connecting to msgQ<%s>\n", this, nam.c_str());
#endif

  size_t isize = sizeof(msq_impl_t);

  bool keep_waiting = true;
#if defined(OSX)
  int shm_id = -1;
  while (keep_waiting) {
    usleep(1 << 18);
    shm_id       = shm_open(_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    keep_waiting = (shm_id < 0);
  }
#else
  while (false == ork::Path(_path).IsFile()) {
    usleep(1 << 18);
  }
  int shm_id = shm_open(_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  OrkAssert(shm_id >= 0);
#endif
  _SHMaddr = mmap(0, isize, PROT_READ | PROT_WRITE, kmapaddrflags, shm_id, 0);
  _inbox   = (msq_impl_t*)_SHMaddr;
  close(shm_id);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQReciever<%p> detected msgQ<%s>\n", this, nam.c_str());
#endif

  keep_waiting = true;
  while (getSenderState() != MessageQueueState::RUNNING) {
    usleep(1 << 19);
    printf("IpcMsgQReciever<%p> waiting for sender to go up\n", this);
  }

  waitSyncStart();
  setRecieverState(MessageQueueState::RUNNING);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQReciever<%p> connected to msgQ<%s>\n", this, nam.c_str());
#endif
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::create(const std::string& nam) {
  setName(nam);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQReciever<%p> creating msgQ<%s>\n", this, nam.c_str());
#endif

  size_t isize = sizeof(msq_impl_t);
  int shm_id   = shm_open(_name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  OrkAssert(shm_id >= 0);
  int iftr = ftruncate(shm_id, isize);
  _SHMaddr = mmap(0, isize, PROT_READ | PROT_WRITE, kmapaddrflags, shm_id, 0);
  new (_SHMaddr) msq_impl_t;
  _inbox = (msq_impl_t*)_SHMaddr;
  close(shm_id);
  setRecieverState(MessageQueueState::RUNNING);
#ifdef __MSGQ_DEBUG__
  printf("IpcMsgQReciever<%p> created msgQ<%s>\n", this, nam.c_str());
#endif
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::waitSyncStart() {
  setRecieverState(MessageQueueState::WAIT);
  // wait for sender to send start/sync message
  ork::ipcq_message_t msg;

  bool bpopped = false;
  while (false == bpopped) {
    bpopped = _inbox->_primaryQ.try_pop(msg);
  }
  ipcq_message_iterator_t syncit(msg);
  std::string sync_content = msg.readString(syncit);
  OrkAssert(sync_content == "start/sync");
  printf("IpcMsgQReciever<%p> got start/sync\n", this);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::setRecieverState(MessageQueueState est) {
  OrkAssert(_inbox != nullptr);
  _inbox->_recieverState.store(est);
}

///////////////////////////////////////////////////////////////////////////////

MessageQueueState IpcMsgQReciever::getSenderState() const {
  OrkAssert(_inbox != nullptr);
  return _inbox->_senderState.load();
}

///////////////////////////////////////////////////////////////////////////////

bool IpcMsgQReciever::tryReceive(ipcq_message_t& out_msg) {
  OrkAssert(_inbox != nullptr);
  bool bpopped = _inbox->_primaryQ.try_pop(out_msg);
  if (bpopped) {
    // out_msg.dump("try_recv");
  }
  return bpopped;
}

///////////////////////////////////////////////////////////////////////////////

bool IpcMsgQReciever::tryReceiveDebug(ipcq_message_t& out_msg) {
  OrkAssert(_inbox != nullptr);
  bool bpopped = _inbox->_debugQ.try_pop(out_msg);
  if (bpopped) {
    // out_msg.dump("try_recv");
  }
  return bpopped;
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::benchSendPerformance(size_t count) {
  // send 16GiB of data
  // measure time
  // print bandwidth

  size_t num_sent          = 0;
  size_t index             = 0;
  const size_t num_to_send = count;

  ork::ipcq_message_t msg;
  for (int i = 0; i < 16; i++) {
    msg.writeString("benchmark");
  }
  size_t msglen = msg.length();
  while (num_sent < num_to_send) {
    send(msg);
    // printf("sent %zu bytes index: %zu\n", msglen, index);
    num_sent += msglen;
    index++;
  }
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQReciever::benchReceivePerformance(size_t count) {
  // receive 16GiB of data
  // measure time
  // print bandwidth

  size_t index             = 0;
  size_t num_recvd         = 0;
  size_t prv_num_recvd     = 0;
  const size_t num_to_recv = count;

  Timer t;
  t.Start();
  while (num_recvd < num_to_recv) {

    ork::ipcq_message_t msg;
    if (tryReceive(msg)) {
      size_t msglen = msg.length();
      // print status every 1MiB

      prv_num_recvd = num_recvd;
      num_recvd += msglen;
      index++;
      // if( (num_recvd>>30) != (prv_num_recvd>>30) ){
      // printf("recvr<%p> received index: %zu bytes: %zu\n", this, index, num_recvd);
      //}
    } else {
      sched_yield();
    }
  }
  double elapsed       = t.SecsSinceStart();
  double bandwidth     = double(num_to_recv) / elapsed;
  double bandwidth_mib = bandwidth / (1024.0 * 1024.0);
  double msgpersec     = double(index) / elapsed;
  double msgpersec_m   = msgpersec / 1e6;
  printf("recvr<%p> received %zu bytes in %f seconds\n", this, num_recvd, elapsed);
  printf("recvr<%p> bandwidth: %f MiB/sec\n", this, bandwidth_mib);
  printf("recvr<%p> million-msg/sec: %f\n", this, msgpersec_m);
}

///////////////////////////////////////////////////////////////////////////////

void IpcMsgQSender::sendDataBlock(datablock_ptr_t dblock) {
  ork::ipcq_message_t msg;
  msg.writeString("datablock");
  msg.writeString("length");
  msg.write<size_t>(dblock->length());
  send(msg);
  size_t index         = 0;
  size_t num_remaining = dblock->length();
  const uint8_t* data  = (const uint8_t*)dblock->data();
  while (num_remaining > 0) {
    msg.clear();
    size_t num_this_iter = std::min(num_remaining, kipcq_message_size);
    msg.writeDataInternal(data + index, num_this_iter);
    send(msg);
    num_remaining -= num_this_iter;
    index += num_this_iter;
  }
  msg.clear();
  msg.writeString("datablock-done");
  send(msg);
}

///////////////////////////////////////////////////////////////////////////////

datablock_ptr_t IpcMsgQReciever::tryReceiveDataBlock() {
  datablock_ptr_t rval;
  ork::ipcq_message_t msg;
  std::vector<uint8_t> buffer;
  if (tryReceive(msg)) {
    ipcq_message_iterator_t iter(msg);
    std::string msgtype = msg.readString(iter);
    OrkAssert(msgtype == "datablock");
    std::string key = msg.readString(iter);
    if (key == "length") {
      size_t length;
      msg.read<size_t>(length, iter);
      rval                 = std::make_shared<DataBlock>();
      size_t index         = 0;
      size_t num_remaining = length;
      // uint8_t* data = (uint8_t*)rval->allocateBlock(length);
      while (num_remaining > 0) {
        ork::ipcq_message_t msg;
        if (tryReceive(msg)) {
          size_t num_this_iter = std::min(num_remaining, kipcq_message_size);
          ipcq_message_iterator_t iter(msg);
          buffer.resize(num_this_iter);
          msg.readDataInternal(buffer.data(), num_this_iter, iter);
          // copy into datablock
          rval->addData(buffer.data(), num_this_iter);
          num_remaining -= num_this_iter;
          index += num_this_iter;
        } else {
          break;
        }
      }
      OrkAssert(num_remaining == 0);
      bool done = false;
      while (not done) {
        ork::ipcq_message_t msg;
        if (tryReceive(msg)) {
          ipcq_message_iterator_t iter(msg);
          std::string msgtype = msg.readString(iter);
          if (msgtype == "datablock-done") {
            done = true;
          }
        }
      }
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

datablock_ptr_t IpcMsgQReciever::receiveDataBlock() {
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
///////////////////////////////////////////////////////////////////////////////

} // namespace ork
