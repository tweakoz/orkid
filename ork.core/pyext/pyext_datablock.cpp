////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <boost/filesystem.hpp>
#include <ork/kernel/environment.h>
#include <ork/event/Event.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/datacache.h>

namespace ork {

void pyinit_datablock(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto dblock_type = py::class_<DataBlock, datablock_ptr_t>(module_core, "DataBlock")
                         ///////////////////
                         .def(py::init<>())
                         ///////////////////
                         .def_static("createFromFile", [](const file::Path& path) -> datablock_ptr_t { //
                            auto dblock = std::make_shared<DataBlock>();
                            ::ork::File infile(path.c_str(), ::ork::EFM_READ);
                            size_t length = 0;
                            infile.GetLength(length);
                            auto buffer = new uint8_t[length];
                            infile.Read(buffer, length);
                            dblock->addData(buffer, length);
                            delete[] buffer;
                            return dblock;
                          })
                         ///////////////////
                         .def_property_readonly("bytes", [](datablock_ptr_t db) -> py::memoryview { //
                           auto as_str = (const char*) db->data();
                           return py::memoryview(py::bytes(as_str, db->length()));
                         })
                         ///////////////////
                         .def(
                             "readByte",
                             [](datablock_ptr_t db, int integer) -> uint8_t {
                               auto as_str = db->data();
                               return as_str[integer];
                             })
                         .def(
                             "writeRawData",
                             [](datablock_ptr_t db, py::bytes data) {
                               auto as_str = std::string(data);
                               db->addData(as_str.c_str(), as_str.length());
                             })
                         .def(
                             "writeBytes",
                             [](datablock_ptr_t db, py::bytes data) {
                               db->addItem<uint64_t>("bytes"_crcu);
                               auto as_str = std::string(data);
                               db->addItem<uint64_t>(as_str.length());
                               db->addData(as_str.c_str(), as_str.length());
                             })
                         .def(
                             "writeString",
                             [](datablock_ptr_t db, std::string str) {
                               db->addItem<uint64_t>("string"_crcu);
                               db->addItem<uint64_t>(str.length()+1);
                               db->addData(str.c_str(), str.length());
                               db->addItem<uint8_t>(0); // null terminator
                             })
                         .def(
                             "writeInt",
                             [](datablock_ptr_t db, int integer) {
                               db->addItem<uint64_t>("int"_crcu);
                               db->addItem<int>(integer);
                             })
                         .def(
                             "writeBuffer",
                             [](datablock_ptr_t db, py::buffer data) {
                               db->addItem<uint64_t>("buffer"_crcu);
                               py::buffer_info info  = data.request();
                               size_t bytes_per_item = 0;
                               if (info.format == py::format_descriptor<uint8_t>::format()) {
                                 bytes_per_item = 1;
                               } else if (info.format == py::format_descriptor<int>::format()) {
                                 bytes_per_item = sizeof(int);
                               } else if (info.format == py::format_descriptor<long>::format()) {
                                 bytes_per_item = sizeof(long);
                               } else if (info.format == py::format_descriptor<float>::format()) {
                                 bytes_per_item = sizeof(float);
                               } else if (info.format == py::format_descriptor<double>::format()) {
                                 bytes_per_item = sizeof(double);
                               } else {
                                 printf("unknown format<%s>\n", info.format.c_str());
                                 OrkAssert(false);
                               }

                               db->addItem<size_t>(info.ndim);

                               auto src = (const uint8_t*)info.ptr;

                               // printf( "ndim<%d>\n", info.ndim );
                               switch (info.ndim) {
                                 case 2: {
                                   size_t length = info.shape[0];
                                   size_t width  = info.shape[1];
                                   db->addItem<size_t>(length);
                                   db->addItem<size_t>(width);
                                   db->addItem<size_t>(bytes_per_item);
                                   size_t numbytes = length * width * bytes_per_item;
                                   db->addData(src, numbytes);
                                   break;
                                 }
                                 case 3: {
                                   size_t length = info.shape[0];
                                   size_t width  = info.shape[1];
                                   size_t depth  = info.shape[2];
                                   db->addItem<size_t>(length);
                                   db->addItem<size_t>(width);
                                   db->addItem<size_t>(depth);
                                   db->addItem<size_t>(bytes_per_item);
                                   size_t numbytes = length * width * depth * bytes_per_item;
                                   db->addData(src, numbytes);
                                   break;
                                 }
                                 default:
                                   OrkAssert(false);
                               }
                             })
                         .def_property_readonly("size", [](datablock_ptr_t db) -> size_t { return db->length(); })
                         .def_property_readonly("hash", [](datablock_ptr_t db) -> uint64_t { return db->hash(); })
                         .def(
                             "__str__",
                             [](datablock_ptr_t db) -> std::string {
                               fxstring<512> fxs;
                               fxs.format("DataBlock(%p) len<%d>", (void*)db.get(), (int)db->length());
                               return fxs.c_str();
                             })
                         .def("__repr__", [](datablock_ptr_t db) -> std::string {
                           fxstring<512> fxs;
                           fxs.format("DataBlock(%p)", (void*)db.get());
                           return fxs.c_str();
                         });
  type_codec->registerStdCodec<datablock_ptr_t>(dblock_type);
  /////////////////////////////////////////////////////////////////////////////////
  using dblockistream_ptr_t = std::shared_ptr<DataBlockInputStream>;
  auto dblockistream_type   = py::class_<DataBlockInputStream, dblockistream_ptr_t>(module_core, "DataBlockInputStream");
  dblockistream_type.def(py::init<>([](datablock_ptr_t dblock) -> dblockistream_ptr_t { //
    return std::make_shared<DataBlockInputStream>(dblock);
  }));
  dblockistream_type.def_property_readonly("length",[](dblockistream_ptr_t dstream) -> size_t { //
    return dstream->length();
  });
  dblockistream_type.def("readInt",[](dblockistream_ptr_t dstream) -> int { //
    auto typecode = dstream->getItem<uint64_t>();
    OrkAssert(typecode == "int"_crcu);
    return dstream->getItem<int>();
  });
  dblockistream_type.def("readBytes",[](dblockistream_ptr_t dstream) -> py::bytes { //
    auto typecode = dstream->getItem<uint64_t>();
    OrkAssert(typecode == "bytes"_crcu);
    auto length = dstream->getItem<uint64_t>();
    //printf ("readBytes length<%d>\n", int(length) );
    auto c_str = (const char*) dstream->current();
    dstream->advance(length);
    return py::bytes(c_str,size_t(length));
  });
  dblockistream_type.def("readString",[](dblockistream_ptr_t dstream) -> py::str { //
    auto typecode = dstream->getItem<uint64_t>();
    OrkAssert(typecode == "string"_crcu);
    auto length = dstream->getItem<uint64_t>();
    //printf ("readString length<%d>\n", int(length) );
    auto c_str = (const char*) dstream->current();
    auto as_str = std::string(c_str);
    //printf ("readString c_str<%s>\n", as_str.c_str() );
    dstream->advance(length);
    return py::str(as_str);
  });
  /////////////////////////////////////////////////////////////////////////////////
  auto dblockcache_type =
      py::class_<DataBlockCache>(module_core, "DataBlockCache")
          .def_static(
              "cachePathForKey",
              [](uint64_t key) -> std::string {
                auto db = DataBlockCache::_generateCachePath(key);
                return db;
              })
          .def_static(
              "findDataBlock",
              [](uint64_t key) -> datablock_ptr_t {
                auto db = DataBlockCache::findDataBlock(key);
                return db;
              })
          .def_static("setDataBlock", [](uint64_t key, datablock_ptr_t db) { DataBlockCache::setDataBlock(key, db); })
          .def_static("removeDataBlock", [](uint64_t key) { DataBlockCache::removeDataBlock(key); })
          .def_property_readonly_static("totalMemoryConsumed", [] -> size_t { return DataBlockCache::totalMemoryConsumed(); });
}

} // namespace ork