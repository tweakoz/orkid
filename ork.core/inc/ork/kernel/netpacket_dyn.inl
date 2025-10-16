////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/netpacket.inl>

namespace ork {

struct DynamicMessagePacket;

///////////////////////////////////////////////////////////////////////////////

struct DynamicMessagePacketIterator final : public MessagePacketIteratorBase {

  DynamicMessagePacketIterator(const DynamicMessagePacket& msg);

  const DynamicMessagePacket& mMessage;
};

///////////////////////////////////////////////////////////////////////////////

struct DynamicMessagePacket final : public MessagePacketBase {

  using iter_t = DynamicMessagePacketIterator;

  ///////////////////////////////////////////////////////

  DynamicMessagePacket();
  iter_t makeIterator() const;
  void writeDataInternal(const void* pdata, size_t ilen) final;
  void readDataInternal(void* pdest, size_t ilen, base_iter_t& it) const final;
  DynamicMessagePacket& operator=(const DynamicMessagePacket& rhs);
  void dump(const char* label);
  const void* data() const final;
  void* data() final;
  size_t length() const final;
  void clear() final;
  size_t max() const final;

  ////////////////////////////////////////////////////

  std::vector<char> _buffer;

};

using dynamic_message_packet_ptr_t = std::shared_ptr<DynamicMessagePacket>;

} // namespace ork
