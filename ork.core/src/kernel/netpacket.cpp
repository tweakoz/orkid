#include <cmath>
#include <limits>
#include <memory>
#include <ork/kernel/netpacket_serdes.inl>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

MessagePacketIteratorBase::MessagePacketIteratorBase(const MessagePacketBase& basepkt)
    : _basepacket(basepkt) //
    , mireadIndex(0) {     //
}
void MessagePacketIteratorBase::clear() {
  mireadIndex = 0;
}
bool MessagePacketIteratorBase::valid() const {
  return mireadIndex < _basepacket.length();
}
int MessagePacketIteratorBase::index() const {
  return mireadIndex;
}
void MessagePacketIteratorBase::skip(int count) {
  mireadIndex += count;
}

///////////////////////////////////////////////////////////////////////////////

MessagePacketBase::~MessagePacketBase() { // virtual
}

void MessagePacketBase::sendZmq(zmq_socket_ptr_t skt) {
  zmq::message_t zmqmsg_send(this->data(), this->length());
  skt->send(zmqmsg_send, zmq::send_flags::dontwait);
}
void MessagePacketBase::recvZmq(zmq_socket_ptr_t skt) {
  zmq::message_t zmqmsg_recv;
  auto recv_status = skt->recv(zmqmsg_recv);
  this->clear();
  this->writeDataInternal(zmqmsg_recv.data(), zmqmsg_recv.size());
}

void MessagePacketBase::writeString(const char* pstr) {
  size_t ilen = strlen(pstr) + 1;
  writeDataInternal(&ilen, sizeof(ilen));
  writeDataInternal(pstr, ilen);
}
void MessagePacketBase::writeString(const std::string& str) {
  size_t ilen = str.length() + 1;
  writeDataInternal(&ilen, sizeof(ilen));
  writeDataInternal(str.c_str(), ilen);
}
////////////////////////////////////////////////////
void MessagePacketBase::writeData(const void* pdata, size_t ilen) {
  write<size_t>(ilen);
  writeDataInternal(pdata, ilen);
}
///////////////////////////////////////////////////////
std::string MessagePacketBase::readString(base_iter_t& it) const {
  size_t ilen = 0;
  readDataInternal((void*)&ilen, sizeof(ilen), it);
  std::vector<char> buffer;
  buffer.resize(ilen);
  readDataInternal((void*)buffer.data(), ilen, it);
  return std::string(buffer.data());
}
void MessagePacketBase::readData(void* pdest, size_t ilen, base_iter_t& it) const {
  size_t rrlen = 0;
  read(rrlen, it);
  assert(rrlen == ilen);
  readDataInternal(pdest, ilen, it);
}

///////////////////////////////////////////////////////////////////////////////

DynamicMessagePacketIterator::DynamicMessagePacketIterator(const DynamicMessagePacket& msg)
    : MessagePacketIteratorBase(msg)
    , mMessage(msg) {
}

DynamicMessagePacket::DynamicMessagePacket() {
}

DynamicMessagePacket::iter_t DynamicMessagePacket::makeIterator() const {
  return DynamicMessagePacket::iter_t(*this);
}

void DynamicMessagePacket::writeDataInternal(const void* pdata, size_t ilen) { // final
  const char* start = (const char*)pdata;
  const char* end   = start + ilen;
  size_t prelen     = length();
  _buffer.insert(_buffer.end(), start, end);
  size_t postlen = length();
  OrkAssert(postlen == (prelen + ilen));
}
void DynamicMessagePacket::readDataInternal(void* pdest, size_t ilen, base_iter_t& it) const { // final
  OrkAssert((it.mireadIndex + ilen) <= length());
  const char* psrc = _buffer.data() + it.mireadIndex;
  memcpy((char*)pdest, psrc, ilen);
  it.mireadIndex += ilen;
}

DynamicMessagePacket& DynamicMessagePacket::operator=(const DynamicMessagePacket& rhs) {

  miSerial   = rhs.miSerial;
  miTimeSent = rhs.miTimeSent;
  _buffer    = rhs._buffer;

  return *this;
}

void DynamicMessagePacket::dump(const char* label) {
  size_t icount = length();
  size_t j      = 0;
  while (icount > 0) {
    uint8_t* paddr = (uint8_t*)(_buffer.data() + j);
    printf("msg<%p:%s> [%02lx : ", this, label, j);
    size_t thisc = (icount >= 16) ? 16 : icount;
    for (size_t i = 0; i < thisc; i++) {
      size_t idx = j + i;
      printf("%02x ", paddr[i]);
    }
    j += thisc;
    icount -= thisc;

    printf("\n");
  }
}
const void* DynamicMessagePacket::data() const { // final
  return (const void*)_buffer.data();
}
void* DynamicMessagePacket::data() { // final
  return (void*)_buffer.data();
}
size_t DynamicMessagePacket::length() const { // final
  return _buffer.size();
}
void DynamicMessagePacket::clear() { // final
  _buffer.clear();
}
size_t DynamicMessagePacket::max() const { // final
  return size_t(1) << 32;
}

} // namespace ork

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork::net::serdes {

Serializer::Serializer(bool std_types) {

  if (not std_types)
    return;

  registerType<std::string>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("std::string");
    msg.writeString(value.get<std::string>());
  });
  registerType<int>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("int");
    msg.template write<int>(value.get<int>());
  });
  registerType<int16vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_ivect = value.get<int16vector_t>();
    msg.writeString("array.int16");
    msg.template write<size_t>(the_ivect.size());
    msg.writeData(the_ivect.data(), the_ivect.size() * sizeof(int16_t));
  });
  registerType<int32_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("int32");
    msg.template write<int32_t>(value.get<int32_t>());
  });
  registerType<int32vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_ivect = value.get<int32vector_t>();
    msg.writeString("array.int32");
    msg.template write<size_t>(the_ivect.size());
    msg.writeData(the_ivect.data(), the_ivect.size() * sizeof(int32_t));
  });
  registerType<int64_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("int64");
    msg.template write<int64_t>(value.get<int64_t>());
  });
  registerType<int64vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_ivect = value.get<int64vector_t>();
    msg.writeString("array.int64");
    msg.template write<size_t>(the_ivect.size());
    msg.writeData(the_ivect.data(), the_ivect.size() * sizeof(int64_t));
  });
  registerType<size_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("size_t");
    msg.template write<size_t>(value.get<size_t>());
  });
  registerType<float>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("float");
    msg.template write<float>(value.get<float>());
  });
  registerType<double>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    msg.writeString("double");
    msg.template write<double>(value.get<double>());
  });
  registerType<float_vect_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvect = value.get<float_vect_t>();
  	printf( "write array.float count<%zu>\n", the_fvect.size() );
    msg.writeString("array.float");
    msg.template write<size_t>(the_fvect.size());
    msg.writeData(the_fvect.data(), the_fvect.size() * sizeof(float));
  });
  registerType<double_vect_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_dvect = value.get<double_vect_t>();
    msg.writeString("array.double");
    msg.template write<size_t>(the_dvect.size());
    msg.writeData(the_dvect.data(), the_dvect.size() * sizeof(double));
  });
  registerType<fvec2>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvec2 = value.get<fvec2>();
    msg.writeString("fvec2");
    msg.template write<float>(the_fvec2.x);
    msg.template write<float>(the_fvec2.y);
  });
  registerType<fvec2vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvec2array = value.get<fvec2vector_t>();
    msg.writeString("array.fvec2");
    msg.template write<size_t>(the_fvec2array.size());
    msg.writeData(the_fvec2array.data(), the_fvec2array.size() * sizeof(fvec2));
  });
  registerType<fvec3>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvec3 = value.get<fvec3>();
    msg.writeString("fvec3");
    msg.template write<float>(the_fvec3.x);
    msg.template write<float>(the_fvec3.y);
    msg.template write<float>(the_fvec3.z);
  });
  registerType<fvec3vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvec3array = value.get<fvec3vector_t>();
    msg.writeString("array.fvec3");
    msg.template write<size_t>(the_fvec3array.size());
    msg.writeData(the_fvec3array.data(), the_fvec3array.size() * sizeof(fvec3));
  });
  registerType<fvec4>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvec4 = value.get<fvec4>();
    msg.writeString("fvec4");
    msg.template write<float>(the_fvec4.x);
    msg.template write<float>(the_fvec4.y);
    msg.template write<float>(the_fvec4.z);
    msg.template write<float>(the_fvec4.w);
  });
  registerType<fvec4vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fvec4array = value.get<fvec4vector_t>();
    msg.writeString("array.fvec4");
    msg.template write<size_t>(the_fvec4array.size());
    msg.writeData(the_fvec4array.data(), the_fvec4array.size() * sizeof(fvec4));
  });
  registerType<fquat>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fquat = value.get<fquat>();
    msg.writeString("fquat");
    msg.template write<float>(the_fquat.x);
    msg.template write<float>(the_fquat.y);
    msg.template write<float>(the_fquat.z);
    msg.template write<float>(the_fquat.w);
  });
  registerType<fquatvector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fquatarray = value.get<fquatvector_t>();
    msg.writeString("array.fquat");
    msg.template write<size_t>(the_fquatarray.size());
    msg.writeData(the_fquatarray.data(), the_fquatarray.size() * sizeof(fquat));
  });
  registerType<fplane3>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    const auto& the_fplane3 = value.get<fplane3>();
    msg.writeString("fplane3");
    msg.template write<float>(the_fplane3.n.x);
    msg.template write<float>(the_fplane3.n.y);
    msg.template write<float>(the_fplane3.n.z);
    msg.template write<float>(the_fplane3.d);
  });
  /*registerType<fplane3vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_fplane3array = value.get<fplane3vector_t>();
    msg.writeString("array.fplane3");
    msg.template write<size_t>(the_fplane3array.size());
    msg.writeData(the_fplane3array.data(), the_fplane3array.size() * sizeof(fplane3));
  });*/
  registerType<fmtx3>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_fmtx3 = value.get<fmtx3>();
    msg.writeString("fmtx3");
    for (int i = 0; i < 9; i++) {
      int x = i % 3;
      int y = i / 3;
      msg.template write<float>(the_fmtx3.elemXY(x, y));
    }
  });
  registerType<fmtx3vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_fmtx3array = value.get<fmtx3vector_t>();
    msg.writeString("array.fmtx3");
    msg.template write<size_t>(the_fmtx3array.size());
    msg.writeData(the_fmtx3array.data(), the_fmtx3array.size() * sizeof(fquat));
  });
  registerType<fmtx4>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_fmtx4 = value.get<fmtx4>();
    msg.writeString("fmtx4");
    for (int i = 0; i < 16; i++) {
      int x = i & 3;
      int y = i >> 2;
      msg.template write<float>(the_fmtx4.elemXY(x, y));
    }
  });
  registerType<fmtx4vector_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_fmtx4array = value.get<fmtx4vector_t>();
    msg.writeString("array.fmtx4");
    msg.template write<size_t>(the_fmtx4array.size());
    msg.writeData(the_fmtx4array.data(), the_fmtx4array.size() * sizeof(fquat));
  });
  registerType<kvmap_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_kvmap = value.get<kvmap_t>();
    msg.writeString("kvmap_t");
    size_t num_items = the_kvmap.size();
    msg.template write<size_t>(num_items);
    for (auto item : the_kvmap) {
      msg.writeString(item.first);
      ser->serialize(msg, item.second);
    }
  });
  registerType<vlist_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_vlist = value.get<vlist_t>();
    msg.writeString("vlist_t");
    size_t num_items = the_vlist.size();
    msg.template write<size_t>(num_items);
    for (auto item : the_vlist) {
      ser->serialize(msg, item);
    }
  });
  registerType<bytes_t>([](Serializer* ser, msgpacketbase_ref_t msg, const val_t& value) {
    auto& the_bytes = value.get<bytes_t>();
    msg.writeString("bytes_t");
    msg.template write<size_t>(the_bytes.size());
    msg.writeData(the_bytes.data(), the_bytes.size());
  });
}

void Serializer::serialize(msgpacketbase_ref_t msg, const val_t& value) {

  auto tname = value.typeName();
  auto it    = _typehandlers.find(tname);
  OrkAssert(it != _typehandlers.end());
  auto serfn = it->second;
  serfn(this, msg, value);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


Deserializer::Deserializer(bool std_types) {
  if (not std_types)
    return;

  registerType("std::string",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
    out_value.template set<std::string>(packet.readString(iter));
  });
  registerType("size_t",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	packet.template read<size_t>(out_value.template make<size_t>(),iter);
  });
  registerType("int",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	packet.template read<int>(out_value.template make<int>(),iter);
  });
  registerType("array.int16",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<int16vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(int16_t),iter);
  });
  registerType("int32",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	packet.template read<int32_t>(out_value.template make<int32_t>(),iter);
  });
  registerType("array.int32",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<int32vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(int32_t),iter);
  });
  registerType("int64",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	packet.template read<int64_t>(out_value.template make<int64_t>(),iter);
  });
  registerType("array.int64",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<int64vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(int64_t),iter);
  });
  registerType("float",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	packet.template read<float>(out_value.template make<float>(),iter);
  });
  registerType("array.float",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<float_vect_t>();
  	out_array.resize(count);
  	printf( "read array.float count<%zu>\n", out_array.size() );
  	packet.readData(out_array.data(), count*sizeof(float),iter);
  });
  registerType("double",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	packet.template read<double>(out_value.template make<double>(),iter);
  });
  registerType("array.double",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<double_vect_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(double),iter);
  });
  registerType("fvec2",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_f2 = out_value.make<fvec2>();
    packet.template read<float>(out_f2.x, iter);
    packet.template read<float>(out_f2.y, iter);
  });
  registerType("array.fvec2",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fvec2vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fvec2),iter);
  });
  registerType("fvec3",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_f3 = out_value.make<fvec3>();
    packet.template read<float>(out_f3.x, iter);
    packet.template read<float>(out_f3.y, iter);
    packet.template read<float>(out_f3.z, iter);
  });
  registerType("array.fvec3",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fvec3vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fvec3),iter);
  });
  registerType("fvec4",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_f4 = out_value.make<fvec4>();
    packet.template read<float>(out_f4.x, iter);
    packet.template read<float>(out_f4.y, iter);
    packet.template read<float>(out_f4.z, iter);
    packet.template read<float>(out_f4.w, iter);
  });
  registerType("array.fvec4",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fvec4vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fvec4),iter);
  });
  registerType("fquat",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_fq = out_value.make<fquat>();
    packet.template read<float>(out_fq.x, iter);
    packet.template read<float>(out_fq.y, iter);
    packet.template read<float>(out_fq.z, iter);
    packet.template read<float>(out_fq.w, iter);
  });
  registerType("array.fquat",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fquatvector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fquat),iter);
  });
  registerType("fplane3",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_fplane = out_value.make<fplane3>();
    packet.template read<float>(out_fplane.n.x, iter);
    packet.template read<float>(out_fplane.n.y, iter);
    packet.template read<float>(out_fplane.n.z, iter);
    packet.template read<float>(out_fplane.d, iter);
  });
  /*registerType("array.fplane3",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fplane3vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fplane3),iter);
  });*/
  registerType("fmtx3",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_fmtx3 = out_value.make<fmtx3>();
    for( int i=0; i<9; i++){
      int x = i%3;
      int y = i/3;
      float element = 0.0f;
      packet.template read<float>(element, iter);
      out_fmtx3.setElemXY(x,y,element);
    }
  });
  registerType("array.fmtx3",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fmtx3vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fmtx3),iter);
  });
  registerType("fmtx4",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	auto& out_fmtx4 = out_value.make<fmtx4>();
    for( int i=0; i<16; i++){
      int x = i&3;
      int y = i>>2;
      float element = 0.0f;
      packet.template read<float>(element, iter);
      out_fmtx4.setElemXY(x,y,element);
    }
  });
  registerType("array.fmtx4",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
  	size_t count = 0;
  	packet.template read<size_t>(count,iter);
  	auto& out_array = out_value.make<fmtx4vector_t>();
  	out_array.resize(count);
  	packet.readData(out_array.data(), count*sizeof(fmtx4),iter);
  });
  registerType("bytes_t",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
    size_t length = 0;
    packet.template read<size_t>(length, iter);
    auto& bytes = out_value.template make<bytes_t>();
    bytes.resize(length);
    packet.readData(bytes.data(), length, iter);
  });
  registerType("vlist_t",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
    size_t num_items = 0;
    packet.template read<size_t>(num_items, iter);
    auto& out_vlist = out_value.template make<vlist_t>();
    out_vlist.resize(num_items);
	  for (size_t i = 0; i < num_items; i++) {
	    deser->deserialize(iter,out_vlist[i],fixupfn);
	  }
  });
  registerType("kvmap_t",[](Deserializer* deser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn) {
  	const auto& packet = iter._basepacket;
    size_t num_items = 0;
    packet.template read<size_t>(num_items, iter);
    auto& out_kvmap = out_value.template make<kvmap_t>();
	  for (size_t i = 0; i < num_items; i++) {
	  	std::string key = packet.readString(iter);
	    deser->deserialize(iter,out_kvmap[key],fixupfn);
	  }
  });


}

	void Deserializer::deserialize(MessagePacketIteratorBase& iter, 
                                 val_t& out_value,
                                 const on_fixup_t& fixupfn){
  	const auto& packet = iter._basepacket;
    auto typecode = packet.readString(iter);
    auto it = _typehandlers.find(typecode);
    OrkAssert(it!=_typehandlers.end());
    auto deserfn = it->second;
    deserfn(this,iter,out_value,fixupfn);
	}

} // namespace ork::net::serdes
