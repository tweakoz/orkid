#pragma once
#include "../file/chunkfile.inl"
#include "../util/crc64.h"
#include "mutex.h"

namespace ork {

struct DataBlock {
  chunkfile::OutputStream _data;
	void reserve(size_t len) { _data.reserve(len); }
	template<typename T> void addItem(const T& item) { _data.AddItem<T>(item); }
	void addData(const void* data, size_t length) { _data.AddData(data,length); }
};

typedef std::shared_ptr<DataBlock> datablockptr_t;
typedef std::unordered_map<uint64_t, datablockptr_t> datablockmap_t;

struct DataBlockMgr {
	//////////////////////////////////////////////////////////////////////////////
  DataBlockMgr() {}
  ~DataBlockMgr() {}
	//////////////////////////////////////////////////////////////////////////////
  static inline datablockptr_t findDataBlock(uint64_t key) {
    auto& inst          = instance();
    datablockptr_t rval = nullptr;
    inst._blockmap.atomicOp([&rval,key](datablockmap_t& m) {
      auto it = m.find(key);
      if (it != m.end()) {
        rval = it->second;
      }
    });
    return rval;
  }
	//////////////////////////////////////////////////////////////////////////////
  static inline void setDataBlock(uint64_t key, datablockptr_t item) {
    auto& inst = instance();
    inst._blockmap.atomicOp([item,key](datablockmap_t& m) {
      auto it = m.find(key);
      assert(it == m.end());
      m[key] = item;
    });
  }
	//////////////////////////////////////////////////////////////////////////////
	static inline void removeDataBlock(uint64_t key) {
    auto& inst = instance();
    inst._blockmap.atomicOp([key](datablockmap_t& m) {
      auto it = m.find(key);
      if(it != m.end())
				m.erase(it);
    });
  }
	//////////////////////////////////////////////////////////////////////////////
	static size_t totalMemoryConsumed() {
		auto& inst = instance();
		size_t rval = 0;
		inst._blockmap.atomicOp([&rval](datablockmap_t& m) {
      for( auto i : m )
				rval += i.second->_data.GetSize();
    });
		return rval;
	}
	//////////////////////////////////////////////////////////////////////////////
  static DataBlockMgr& instance() {
    static DataBlockMgr _mgr;
    return _mgr;
  }
	//////////////////////////////////////////////////////////////////////////////
  LockedResource<datablockmap_t> _blockmap;
};

}
