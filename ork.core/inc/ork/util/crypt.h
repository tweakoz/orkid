////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/datablock.h>
#include <sodium.h>
#include <memory>
#include <string>

namespace ork::util::crypt {

////////////////////////////////////////////////////////////////////////////////
// Secure buffer that zeros memory on destruction
////////////////////////////////////////////////////////////////////////////////

class SecureBuffer {
  std::vector<uint8_t> _data;
public:
  explicit SecureBuffer(size_t size);
  SecureBuffer(const std::string& password, const std::string& salt);
  ~SecureBuffer();
  
  // Delete copy constructor and assignment to prevent accidental copies
  SecureBuffer(const SecureBuffer&) = delete;
  SecureBuffer& operator=(const SecureBuffer&) = delete;
  
  // Allow move semantics
  SecureBuffer(SecureBuffer&&) = default;
  SecureBuffer& operator=(SecureBuffer&&) = default;
  
  uint8_t* data() { return _data.data(); }
  const uint8_t* data() const { return _data.data(); }
  size_t size() const { return _data.size(); }
};

////////////////////////////////////////////////////////////////////////////////
// Libsodium ChaCha20-Poly1305 AEAD codec
////////////////////////////////////////////////////////////////////////////////

class LibsodiumCodec : public EncryptionCodec {
  SecureBuffer _key;
  std::string _context;  // For deterministic nonce generation
  
public:
  explicit LibsodiumCodec(const SecureBuffer& key, const std::string& context = "");
  LibsodiumCodec(const std::string& password, const std::string& context);
  
  datablock_ptr_t encrypt(const DataBlock* inp) override;
  datablock_ptr_t decrypt(const DataBlock* inp) override;
  
  // Chunk-aware operations with deterministic nonces
  datablock_ptr_t encryptChunk(const DataBlock* inp, uint64_t chunk_index);
  datablock_ptr_t decryptChunk(const DataBlock* inp, uint64_t chunk_index);
  
  // Get algorithm info
  static std::string algorithmName() { return "chacha20poly1305"; }
  static size_t keySize() { return crypto_aead_chacha20poly1305_IETF_KEYBYTES; }
  static size_t nonceSize() { return crypto_aead_chacha20poly1305_IETF_NPUBBYTES; }
  static size_t tagSize() { return crypto_aead_chacha20poly1305_IETF_ABYTES; }
  
private:
  void generateNonce(uint8_t* nonce, uint64_t counter = 0) const;
};

using libsodiumcodec_ptr_t = std::shared_ptr<LibsodiumCodec>;
using encryptioncodec_ptr_t = std::shared_ptr<EncryptionCodec>;

////////////////////////////////////////////////////////////////////////////////
// Factory helpers
////////////////////////////////////////////////////////////////////////////////

libsodiumcodec_ptr_t createCodec(const std::string& password, const std::string& namespace_id);
libsodiumcodec_ptr_t createCodecFromKey(const SecureBuffer& key, const std::string& context = "");

// Initialize libsodium (called automatically, but can be called explicitly)
bool initializeCrypto();

////////////////////////////////////////////////////////////////////////////////
// File operations for convenience
////////////////////////////////////////////////////////////////////////////////

void encryptFile(const file::Path& input, const file::Path& output, encryptioncodec_ptr_t codec);
void decryptFile(const file::Path& input, const file::Path& output, encryptioncodec_ptr_t codec);

} // namespace ork::util::crypt