#pragma once
#include "../file/chunkfile.inl"
#include "../file/fileenv.h"
#include "../util/crc64.h"
#include "environment.h"
#include "mutex.h"
#include <boost/filesystem.hpp>

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
	static std::string _generateCachePath(uint64_t key) {
		using namespace boost::filesystem;
		std::string cache_dir;
		genviron.get("OBT_STAGE",cache_dir);
		cache_dir = cache_dir+"/dblockcache";
		if(false == exists(cache_dir)){
			printf( "Making cache_dir folder<%s>\n", cache_dir.c_str() );
			create_directory( cache_dir );
		}
		auto cache_path = cache_dir+"/"+FormatString("%zx.bin",key);
		return cache_path;
	}
	//////////////////////////////////////////////////////////////////////////////
  static inline datablockptr_t findDataBlock(uint64_t key) {
    auto& inst          = instance();
    datablockptr_t rval = nullptr;
		auto cache_path = _generateCachePath(key);
    inst._blockmap.atomicOp([&rval,key,cache_path](datablockmap_t& m) {
      auto it = m.find(key);
      if (it == m.end()) {
				using namespace boost::filesystem;
				if( exists(cache_path) ){
					rval = std::make_shared<DataBlock>();
					size_t len = file_size(cache_path);
					rval->reserve(len);
					FILE* fin = fopen(cache_path.c_str(), "rb");
					void* pdata = malloc(len);
					fread(pdata,len,1,fin);
					fclose(fin);
					rval->addData(pdata,len);
					free(pdata);
					m[key] = rval;
				}
			}
			else {
        rval = it->second;
      }
    });
    return rval;
  }
	//////////////////////////////////////////////////////////////////////////////
  static inline void setDataBlock(uint64_t key, datablockptr_t item) {
    auto& inst = instance();
		auto cache_path = _generateCachePath(key);
    inst._blockmap.atomicOp([item,key,cache_path](datablockmap_t& m) {
      auto it = m.find(key);
      assert(it == m.end());
      m[key] = item;
			using namespace boost::filesystem;
			if( false == exists(cache_path) ){
				FILE* fout = fopen(cache_path.c_str(), "wb");
				fwrite(item->_data.GetData(), item->_data.GetSize(), 1, fout );
				fclose(fout);
			}

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
