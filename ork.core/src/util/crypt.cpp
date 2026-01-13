////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/crypt.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>
#include <cstring>

namespace ork::util::crypt {

static logchannel_ptr_t logchan_crypt = logger()->configureChannel("CRYPT", fvec3(0.8f, 0.4f, 0.8f), true);

////////////////////////////////////////////////////////////////////////////////
// Initialize libsodium
////////////////////////////////////////////////////////////////////////////////

static std::once_flag g_sodium_init_flag;
static bool g_sodium_initialized = false;

bool initializeCrypto() {
  std::call_once(g_sodium_init_flag, []() {
    if (sodium_init() < 0) {
      logchan_crypt->log("ERROR: Failed to initialize libsodium");
      g_sodium_initialized = false;
    } else {
      logchan_crypt->log("Libsodium initialized successfully");
      g_sodium_initialized = true;
    }
  });
  return g_sodium_initialized;
}

////////////////////////////////////////////////////////////////////////////////
// SecureBuffer implementation
////////////////////////////////////////////////////////////////////////////////

SecureBuffer::SecureBuffer(size_t size) : _data(size) {
  initializeCrypto();
  // Data is already zero-initialized by vector
}

SecureBuffer::SecureBuffer(const std::string& password, const std::string& salt) {
  initializeCrypto();
  
  // Use Argon2id for key derivation
  _data.resize(crypto_aead_chacha20poly1305_IETF_KEYBYTES);
  
  // Create a properly sized salt buffer
  unsigned char salt_buffer[crypto_pwhash_SALTBYTES];
  memset(salt_buffer, 0, crypto_pwhash_SALTBYTES);
  
  // Copy the salt string, truncating or padding as needed
  size_t salt_copy_len = std::min(salt.length(), (size_t)crypto_pwhash_SALTBYTES);
  memcpy(salt_buffer, salt.c_str(), salt_copy_len);
    
  if (crypto_pwhash(
      _data.data(), _data.size(),
      password.c_str(), password.length(),
      salt_buffer,
      crypto_pwhash_OPSLIMIT_MODERATE,
      crypto_pwhash_MEMLIMIT_MODERATE,
      crypto_pwhash_ALG_ARGON2ID13) != 0) {
    throw std::runtime_error("Key derivation failed");
  }
    
  if(0)logchan_crypt->log("Derived key from password for context: %s", salt.c_str());
}

SecureBuffer::~SecureBuffer() {
  // Securely wipe the key data
  if (!_data.empty()) {
    sodium_memzero(_data.data(), _data.size());
  }
}

////////////////////////////////////////////////////////////////////////////////
// LibsodiumCodec implementation
////////////////////////////////////////////////////////////////////////////////

LibsodiumCodec::LibsodiumCodec(const SecureBuffer& key, const std::string& context)
    : _key(key.size()), _context(context) {
  initializeCrypto();
  
  if (key.size() != keySize()) {
    throw std::runtime_error(FormatString("Invalid key size: %zu (expected %zu)", 
                                          key.size(), keySize()));
  }
  
  // Copy key data
  std::memcpy(_key.data(), key.data(), key.size());
}

LibsodiumCodec::LibsodiumCodec(const std::string& password, const std::string& context)
    : _key(password, context), _context(context) {
  initializeCrypto();
}

void LibsodiumCodec::generateNonce(uint8_t* nonce, uint64_t counter) const {
  // Generate deterministic nonce from context and counter
  // This ensures same input always produces same output (important for caching)
  
  crypto_generichash_state state;
  crypto_generichash_init(&state, nullptr, 0, nonceSize());
  
  // Hash context
  crypto_generichash_update(&state, 
                            reinterpret_cast<const unsigned char*>(_context.c_str()), 
                            _context.length());
  
  // Hash counter
  crypto_generichash_update(&state, 
                            reinterpret_cast<const unsigned char*>(&counter), 
                            sizeof(counter));
  
  // Generate nonce
  crypto_generichash_final(&state, nonce, nonceSize());
}

datablock_ptr_t LibsodiumCodec::encrypt(const DataBlock* inp) {
  return encryptChunk(inp, 0);
}

datablock_ptr_t LibsodiumCodec::encryptChunk(const DataBlock* inp, uint64_t chunk_index) {
  if (!inp) {
    throw std::invalid_argument("Cannot encrypt null DataBlock");
  }
  
  // Create output datablock
  // Format: [nonce][ciphertext+tag]
  size_t ciphertext_len = inp->length() + tagSize();
  size_t total_len = nonceSize() + ciphertext_len;
  
  auto output = std::make_shared<DataBlock>();
  output->reserve(total_len);
  
  // Generate nonce
  uint8_t nonce[nonceSize()];
  generateNonce(nonce, chunk_index);
  
  // Add nonce to output
  output->addData(nonce, nonceSize());
  
  // Allocate space for ciphertext
  uint8_t* ciphertext = static_cast<uint8_t*>(output->allocateBlock(ciphertext_len));
  
  // Encrypt
  unsigned long long actual_len;
  if (crypto_aead_chacha20poly1305_ietf_encrypt(
      ciphertext, &actual_len,
      inp->data(), inp->length(),
      nullptr, 0,  // No additional data
      nullptr,     // No secret nonce
      nonce,
      _key.data()) != 0) {
    throw std::runtime_error("Encryption failed");
  }
  
  OrkAssert(actual_len == ciphertext_len);
  
  logchan_crypt->log("Encrypted chunk %zu: %zu -> %zu bytes", 
                     chunk_index, inp->length(), output->length());
  
  return output;
}

datablock_ptr_t LibsodiumCodec::decrypt(const DataBlock* inp) {
  return decryptChunk(inp, 0);
}

datablock_ptr_t LibsodiumCodec::decryptChunk(const DataBlock* inp, uint64_t chunk_index) {
  if (!inp) {
    throw std::invalid_argument("Cannot decrypt null DataBlock");
  }
  
  
  if (inp->length() < nonceSize() + tagSize()) {
    throw std::runtime_error("Input too small to be encrypted data");
  }
  
  // Extract nonce
  const uint8_t* nonce = inp->data();
  
 
  // Verify nonce matches expected
  uint8_t expected_nonce[nonceSize()];
  generateNonce(expected_nonce, chunk_index);
    
  if (sodium_memcmp(nonce, expected_nonce, nonceSize()) != 0) {
    throw std::runtime_error("Nonce mismatch - possible corruption or wrong chunk index");
  }
  
  // Extract ciphertext
  const uint8_t* ciphertext = inp->data() + nonceSize();
  size_t ciphertext_len = inp->length() - nonceSize();
  size_t plaintext_len = ciphertext_len - tagSize();
    
  // Create output datablock
  auto output = std::make_shared<DataBlock>();
  output->reserve(plaintext_len);
  uint8_t* plaintext = static_cast<uint8_t*>(output->allocateBlock(plaintext_len));
  
  // Decrypt
  unsigned long long actual_len;
  int result = crypto_aead_chacha20poly1305_ietf_decrypt(
      plaintext, &actual_len,
      nullptr,  // No secret nonce
      ciphertext, ciphertext_len,
      nullptr, 0,  // No additional data
      nonce,
      _key.data());
        
  if (result != 0) {
    throw std::runtime_error("Decryption failed - authentication tag mismatch");
  }
  
  OrkAssert(actual_len == plaintext_len);
  
  if(0)logchan_crypt->log("Decrypted chunk %zu: %zu -> %zu bytes", 
                     chunk_index, inp->length(), output->length());
  
  return output;
}

////////////////////////////////////////////////////////////////////////////////
// Factory functions
////////////////////////////////////////////////////////////////////////////////

libsodiumcodec_ptr_t createCodec(const std::string& password, const std::string& namespace_id) {
  return std::make_shared<LibsodiumCodec>(password, namespace_id);
}

libsodiumcodec_ptr_t createCodecFromKey(const SecureBuffer& key, const std::string& context) {
  return std::make_shared<LibsodiumCodec>(key, context);
}

////////////////////////////////////////////////////////////////////////////////
// File operations
////////////////////////////////////////////////////////////////////////////////

void encryptFile(const file::Path& input, const file::Path& output, encryptioncodec_ptr_t codec) {
  auto data = DataBlock::createFromPath(input.c_str());
  auto encrypted = data->encrypt(codec);
  
  File outputFile(output, EFM_WRITE);
  outputFile.Write(encrypted->data(), encrypted->length());
  
  logchan_crypt->log("Encrypted file: %s -> %s (%zu -> %zu bytes)", 
                     input.c_str(), output.c_str(), 
                     data->length(), encrypted->length());
}

void decryptFile(const file::Path& input, const file::Path& output, encryptioncodec_ptr_t codec) {
  auto data = DataBlock::createFromPath(input.c_str());
  auto decrypted = data->decrypt(codec);
  
  File outputFile(output, EFM_WRITE);
  outputFile.Write(decrypted->data(), decrypted->length());
  
  logchan_crypt->log("Decrypted file: %s -> %s (%zu -> %zu bytes)", 
                     input.c_str(), output.c_str(), 
                     data->length(), decrypted->length());
}

} // namespace ork::util::crypt