////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/crypt.h>
#include <ork/kernel/datablock.h>
#include <ork/util/xxhash.inl>

namespace ork::util::crypt {

void pyinit_crypt(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();
  
  // Module level initialization
  module_core.def("initialize_crypto", &initializeCrypto, 
    "Initialize libsodium crypto library");

  /////////////////////////////////////////////////////////////////////////////////
  // SecureBuffer - Note: we expose a wrapper to handle move semantics
  /////////////////////////////////////////////////////////////////////////////////
  
  struct PySecureBuffer {
    std::unique_ptr<SecureBuffer> buffer;
    
    PySecureBuffer(size_t size) : buffer(std::make_unique<SecureBuffer>(size)) {}
    PySecureBuffer(const std::string& password, const std::string& salt) 
      : buffer(std::make_unique<SecureBuffer>(password, salt)) {}
    
    size_t size() const { return buffer->size(); }
  };
  
  using pysecurebuffer_ptr_t = std::shared_ptr<PySecureBuffer>;
  
  auto securebuffer_type = py::class_<PySecureBuffer, pysecurebuffer_ptr_t>(module_core, "SecureBuffer")
    .def(py::init<size_t>(), py::arg("size"))
    .def(py::init<const std::string&, const std::string&>(), 
         py::arg("password"), py::arg("salt"))
    .def("size", &PySecureBuffer::size)
    .def_static("from_password", [](const std::string& password, const std::string& salt) {
      return std::make_shared<PySecureBuffer>(password, salt);
    })
    .def("__repr__", [](const PySecureBuffer& self) -> std::string {
      return FormatString("SecureBuffer(size=%zu)", self.size());
    });
  
  /////////////////////////////////////////////////////////////////////////////////
  // Base EncryptionCodec
  /////////////////////////////////////////////////////////////////////////////////
  
  py::class_<EncryptionCodec, encryptioncodec_ptr_t>(module_core, "EncryptionCodec")
    .def("encrypt", [](EncryptionCodec& self, py::bytes data) -> py::bytes {
      std::string input_str = data;
      auto input_block = std::make_shared<DataBlock>(input_str.data(), input_str.size());
      auto encrypted = self.encrypt(input_block.get());
      if (encrypted) {
        return py::bytes(reinterpret_cast<const char*>(encrypted->data()), encrypted->length());
      }
      return py::bytes();
    }, py::arg("data"))
    .def("decrypt", [](EncryptionCodec& self, py::bytes data) -> py::bytes {
      std::string input_str = data;
      auto input_block = std::make_shared<DataBlock>(input_str.data(), input_str.size());
      auto decrypted = self.decrypt(input_block.get());
      if (decrypted) {
        return py::bytes(reinterpret_cast<const char*>(decrypted->data()), decrypted->length());
      }
      return py::bytes();
    }, py::arg("data"));
  
  /////////////////////////////////////////////////////////////////////////////////
  // LibsodiumCodec
  /////////////////////////////////////////////////////////////////////////////////
  
  auto libsodium_type = py::class_<LibsodiumCodec, EncryptionCodec, 
                                    libsodiumcodec_ptr_t>(module_core, "LibsodiumCodec")
    .def(py::init([](pysecurebuffer_ptr_t key, const std::string& context) {
      return std::make_shared<LibsodiumCodec>(*key->buffer, context);
    }), py::arg("key"), py::arg("context") = "")
    .def(py::init<const std::string&, const std::string&>(),
         py::arg("password"), py::arg("context"))
    .def("encrypt_chunk", [](LibsodiumCodec& self, datablock_ptr_t data, uint64_t chunk_index) {
      return self.encryptChunk(data.get(), chunk_index);
    }, py::arg("data"), py::arg("chunk_index"))
    .def("decrypt_chunk", [](LibsodiumCodec& self, datablock_ptr_t data, uint64_t chunk_index) {
      return self.decryptChunk(data.get(), chunk_index);
    }, py::arg("data"), py::arg("chunk_index"))
    .def_static("algorithm_name", &LibsodiumCodec::algorithmName)
    .def_static("key_size", &LibsodiumCodec::keySize)
    .def_static("nonce_size", &LibsodiumCodec::nonceSize)
    .def_static("tag_size", &LibsodiumCodec::tagSize)
    .def("__repr__", [](const LibsodiumCodec& self) -> std::string {
      return FormatString("LibsodiumCodec(algorithm='%s')", 
                          LibsodiumCodec::algorithmName().c_str());
    });
  
  /////////////////////////////////////////////////////////////////////////////////
  // Factory functions
  /////////////////////////////////////////////////////////////////////////////////
  
  module_core.def("create_codec", &createCodec, 
    py::arg("password"), py::arg("namespace_id"),
    "Create a codec from password and namespace");
    
  module_core.def("create_codec_from_key", 
    [](pysecurebuffer_ptr_t key, const std::string& context) {
      return createCodecFromKey(*key->buffer, context);
    },
    py::arg("key"), py::arg("context") = "",
    "Create a codec from a SecureBuffer key");
  
  /////////////////////////////////////////////////////////////////////////////////
  // File operations
  /////////////////////////////////////////////////////////////////////////////////
  
  module_core.def("encrypt_file", 
    [](const std::string& input, const std::string& output, encryptioncodec_ptr_t codec) {
      encryptFile(file::Path(input), file::Path(output), codec);
    },
    py::arg("input"), py::arg("output"), py::arg("codec"),
    "Encrypt a file using the specified codec");
    
  module_core.def("decrypt_file",
    [](const std::string& input, const std::string& output, encryptioncodec_ptr_t codec) {
      decryptFile(file::Path(input), file::Path(output), codec);
    },
    py::arg("input"), py::arg("output"), py::arg("codec"),
    "Decrypt a file using the specified codec");
  
  /////////////////////////////////////////////////////////////////////////////////
  // XXH64HASH - Fast non-cryptographic hash
  /////////////////////////////////////////////////////////////////////////////////
  
  using xxh64hash_ptr_t = std::shared_ptr<XXH64HASH>;
  
  auto xxh64_type = py::class_<XXH64HASH, xxh64hash_ptr_t>(module_core, "XXH64HASH")
    .def(py::init<>())
    .def("init", &XXH64HASH::init)
    .def("finish", &XXH64HASH::finish)
    .def("result", &XXH64HASH::result)
    .def("accumulate", [](xxh64hash_ptr_t self, py::bytes data) {
      std::string input_str = data;
      self->accumulate(input_str.data(), input_str.size());
    }, py::arg("data"))
    .def("accumulateString", &XXH64HASH::accumulateString, py::arg("item"))
    .def("digestString", [](xxh64hash_ptr_t self) -> std::string {
      char hash_str[17];
      snprintf(hash_str, sizeof(hash_str), "%016llx", (unsigned long long)self->result());
      return std::string(hash_str);
    }, "Get the hash result as a hex string")
    .def("__repr__", [](xxh64hash_ptr_t self) -> std::string {
      return FormatString("XXH64HASH(result=%016llx)", (unsigned long long)self->result());
    });
  type_codec->registerStdCodec<xxh64hash_ptr_t>(xxh64_type);
  
  /////////////////////////////////////////////////////////////////////////////////
  // Note: DataBlock methods are extended where DataBlock is defined
  // We'll add compression support when we implement it in DataBlock itself
  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::util::crypt