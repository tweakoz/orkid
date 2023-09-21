#pragma once
///////////////////////////////////////////////////////////////////////////////
#include <zmq.hpp>
#include <zmq_addon.hpp>
///////////////////////////////////////////////////////////////////////////////
namespace ork::zeromq {
///////////////////////////////////////////////////////////////////////////////

struct Context;
struct Socket;

using context_ptr_t    = std::shared_ptr<Context>;
using context_rawptr_t = Context*;
using socket_ptr_t     = std::shared_ptr<Socket>;

using impl_context_t     = zmq::context_t;
using impl_context_ptr_t = std::shared_ptr<impl_context_t>;
using impl_socket_t      = zmq::socket_t;
using impl_socket_ptr_t  = std::shared_ptr<impl_socket_t>;

///////////////////////////////////////////////////////////////////////////////

struct Context {

  Context(int numiothread = 4);
  ~Context();
  void close();
  socket_ptr_t createSocket(zmq::socket_type type,std::string name="none");
  void removeSocket(socket_ptr_t skt);

  void dumpOpenSockets() const;

  impl_context_ptr_t _impl;
  std::unordered_set<socket_ptr_t> _socket_tracker;
  std::atomic<int> _socket_count;
};

///////////////////////////////////////////////////////////////////////////////

struct Socket {

  Socket(context_rawptr_t context, zmq::socket_type type);
  ~Socket();
  void close();
  void bind(const std::string& addr);
  void connect(const std::string& addr);
  void subscribe(const std::string& topic);
  void setConnTimeout(int timeout); // mSec
  void setTimeout(int timeout);
  void setReceiveHighWaterMark(int hwm);
  void setSendHighWaterMark(int hwm);
  void setHighWaterMark(int hwm);

  impl_socket_ptr_t _impl;
  context_rawptr_t _context;
  bool _closed = true;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

inline Context::Context(int numiothread) {
  _impl         = std::make_shared<impl_context_t>(numiothread);
  _socket_count = 0;
}
inline Context::~Context() {
  for (auto item : _socket_tracker) {
    item->close();
  }
  _socket_tracker.clear();
  _impl->close();
}

inline void Context::close() {
  _impl->close();
}

inline socket_ptr_t Context::createSocket(zmq::socket_type type,std::string name) {
  auto socket = std::make_shared<Socket>(this, type);
  _socket_tracker.insert(socket);
  socket->_name = name;
  return socket;
}
inline void Context::removeSocket(socket_ptr_t skt) {
  auto it = _socket_tracker.find(skt);
  OrkAssert(it != _socket_tracker.end());
  _socket_tracker.erase(it);
  skt->close();
}
inline void Context::dumpOpenSockets() const{
    printf( "DUMP OPEN SOCKETS<%p>\n", this );
    for( auto item : _socket_tracker ){
        if( item->_closed == false){
            printf( "socket<%p:%s>\n", item.get(), item->_name.c_str() );
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

inline Socket::Socket(context_rawptr_t context, zmq::socket_type type)
    : _context(context) {
  _impl = std::make_shared<impl_socket_t>(*(context->_impl), type);
}
inline Socket::~Socket() {
  _impl->close();
}
inline void Socket::close() {
  if (not _closed) {
    _context->_socket_count--;
    printf( "CLOSE SOCKET<%p:%s> num open<%d>\n", this, _name.c_str(), _context->_socket_count.load() );
    _impl->close();
    _closed = true;
  }
}
inline void Socket::bind(const std::string& addr) {
  _impl->bind(addr.c_str());
  _context->_socket_count++;
  _closed = false;
}
inline void Socket::connect(const std::string& addr) {
  _impl->connect(addr.c_str());
  _context->_socket_count++;
  _closed = false;
}
inline void Socket::subscribe(const std::string& topic) {
  _impl->setsockopt(ZMQ_SUBSCRIBE, topic.c_str(), topic.length());
}

inline void Socket::setConnTimeout(int timeout) { // mSec
  _impl->setsockopt(ZMQ_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
}
inline void Socket::setTimeout(int timeout) {
  _impl->setsockopt(ZMQ_RCVTIMEO, timeout);
  _impl->setsockopt(ZMQ_SNDTIMEO, timeout);
}
inline void Socket::setReceiveHighWaterMark(int hwm) {
  _impl->set(zmq::sockopt::rcvhwm, hwm);
}
inline void Socket::setSendHighWaterMark(int hwm) {
  _impl->set(zmq::sockopt::sndhwm, hwm);
}
inline void Socket::setHighWaterMark(int hwm) {
  _impl->setsockopt(ZMQ_SNDHWM, hwm);
  _impl->setsockopt(ZMQ_RCVHWM, hwm);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::zeromq