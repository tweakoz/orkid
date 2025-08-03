///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2025, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <ork/util/ipcq.h>
#include <ork/file/path.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/timer.h>

#define __MSGQ_DEBUG__

// Include the template implementation
#include <ork/util/ipcq.inl>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// Explicit template instantiations
///////////////////////////////////////////////////////////////////////////////

// Tiny: 64 byte messages, 1024 queue size
template struct IpcqShmObjectImage<64, 1024>;
template struct IpcMsgQSender<64, 1024>;
template struct IpcMsgQReciever<64, 1024>;

// Small: 256 byte messages, 4096 queue size
template struct IpcqShmObjectImage<256, 4096>;
template struct IpcMsgQSender<256, 4096>;
template struct IpcMsgQReciever<256, 4096>;

// Standard: 1024 byte messages, 8192 queue size
template struct IpcqShmObjectImage<1024, 8192>;
template struct IpcMsgQSender<1024, 8192>;
template struct IpcMsgQReciever<1024, 8192>;

// Large: 4096 byte messages, 4096 queue size
template struct IpcqShmObjectImage<4096, 4096>;
template struct IpcMsgQSender<4096, 4096>;
template struct IpcMsgQReciever<4096, 4096>;

// Huge: 16384 byte messages, 1024 queue size
template struct IpcqShmObjectImage<16384, 1024>;
template struct IpcMsgQSender<16384, 1024>;
template struct IpcMsgQReciever<16384, 1024>;

// Gargantuan: 65536 byte messages, 1024 queue size
template struct IpcqShmObjectImage<65536, 1024>;
template struct IpcMsgQSender<65536, 1024>;
template struct IpcMsgQReciever<65536, 1024>;

// Titan: 262144 byte messages, 1024 queue size
template struct IpcqShmObjectImage<262144, 1024>;
template struct IpcMsgQSender<262144, 1024>;
template struct IpcMsgQReciever<262144, 1024>;

// Colossal: 524288 byte messages, 1024 queue size
template struct IpcqShmObjectImage<524288, 1024>;
template struct IpcMsgQSender<524288, 1024>;
template struct IpcMsgQReciever<524288, 1024>;

// Massive: 1048576 byte messages, 1024 queue size
template struct IpcqShmObjectImage<1048576, 1024>;
template struct IpcMsgQSender<1048576, 1024>;
template struct IpcMsgQReciever<1048576, 1024>;

// Legacy compatibility (matches old hardcoded sizes)
template struct IpcqShmObjectImage<256, 8192>;
template struct IpcMsgQSender<256, 8192>;
template struct IpcMsgQReciever<256, 8192>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork