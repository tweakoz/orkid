# DataBlocks and the DataBlockCache

Orkid supports a standardized methodology for processing source asset data (eg GLTF/GLB models, PNG textures, etc..) into cooked asset data (eg. Orkid format models and textures), and caching the results for later use.


Reasons behind this methodology include:

1. Faster loading times - don't repeatedly process data which could be processed once.
2. Faster network transmission - often source assets are smaller than the binary runtime assets (eg png or jpg vs gpu block compressed textures). 
3. Ease of development - the developer typically wants to just think in terms of source assets.

Asset conversion for common types is handled by the engine. It is implemented with hashing and content addressable filesystem techniques. For performance reasons, the hashing mechanism currently uses CRC64, so collision rates will match that of CRC64. I personally do not worry about it, but if you do worry about that sort of thing and your application really requires that guarantee, replacing the hash algorithm for the datablock cache is very simple.

Future plans include allowing the developer to bypass the source asset hashing through the use of a *manifest* and a pregenerated datablock-cache.

For 3d models and textures, and a few other asset types the datablock cache is already integrated into those asset managers. 

For security reasons, caching can be disabled by setting the environment variable ```ORKID_DISABLE_DBLOCK_CACHING``` 

For custom developer implemented asset types, the following example should give you the general idea of how to implement caching.


### Custom datablock cache example

```cpp
using namespace ork;

datablock_ptr_t loadMyProcessedAsset( std::vector<uint8_t> src_data ) { // src_data: some arbitrary source data filled in somewhere else

  auto datahasher = DataBlock::createHasher();
  datahasher->accumulateString("my-asset-type");               // type code
  datahasher->accumulateItem<float>(1.0);   		       // version code
  datahasher->accumulate(src_data.data(),src_data.length());   // source content
  datahasher->finish();
  uint64_t datahash = datahasher->result();

  datablock_ptr_t dblock = DataBlockCache::findDataBlock(datahash); // search cache

  if(dblock){ // processed content was found 
    // data is ready for re-use and nothing left to do.
  }
  else { // processed content was NOT found - we need to regenerate it
	
    dblock = std::make_shared<DataBlock>(); // create a new datablock
	  
    dblock->reserve(sizeof(uint64_t)*16); // reserve 128 byes

    // for this example we will just fill with 0's
    //  but a real world example would do something like compress textures,
    //   generate and compress PBR-irradiance texure-maps, process a mesh, heightmap, etc...
    for(int i=0; i<16; i++)
      dblock->addItem<uint64_t>(0); 
	
    DataBlockCache::setDataBlock(datahash, dblock); // cache the datablock for later use
	
  }
  
  return dblock;
 }

//////////////////////

void test() {
  std::vector<uint8_t> src_data; // some arbitrary source data filled in somewhere else
   
  auto processed = loadMyProcessedAsset(src_data);
  size_t processed_data_length = processed->length();
  const uint8_t* processed_data = processed->data();

  // Load the processed data..
	
}

```

---
---


# ChunkFiles

Chunkfiles provide another layer of developer convenience when reading, writing and processing asset data. I commonly embed chunkfiles into datablocks. They are a modernized c++ centric adaptation of old school EA IFF chunkfiles. The word *file* in chunkfile is a bit of a misnomer, since in orkid, chunkfiles are just formatted datablocks and datablocks may come from files or maybe from network packets or other sources. Part of the convience comes from the fact you can read/write from/to multiple streams in any order and the streams will each occupy contiguous chunks of the file/datablock - this makes for cleaner code when keeping CPU bound header data separate from GPU bound data.

* example datablock/chunk writer

```cpp

datablock_ptr_t gpu_data_writer() {

  constexpr size_t block_count = 8;
  uint64_t src_content_hashkey = 0x1234567812345678; // compute real hash key from source data
  std::vector<uint8_t> gpu_data[block_count]; some arbitrary GPU data filled in somewhere else

   
  chunkfile::Writer chunkwriter("mygpuchunkformat");
  auto hdrstream = chunkwriter.AddStream("header");
  auto gpustream = chunkwriter.AddStream("gpudata");
 
  hdrstream->AddItem<uint64_t>("gpublockcount"_crcu); // key 
  hdrstream->AddItem<uint64_t>(block_count);          // value 

  for(size_t i=0; i<block_count; i++){
    const auto& gpudataitem = gpu_data[i];
    // write header information
    hdrstream->AddItem<uint64_t>("gpudataoffset"_crcu); // key 
    hdrstream->AddItem<uint64_t>(gpustream->GetSize()); // value
    hdrstream->AddItem<uint64_t>("gpudatalength"_crcu); // key 
    hdrstream->AddItem<uint64_t>(gpudataitem.length()); // value
	
    // write gpu block information

    gpustream->Write(gpudataitem.data(),gpudataitem.length()); // write gpu block data item
  }
	
  dblock = std::make_shared<DataBlock>();
  chunkwriter.writeToDataBlock(dblock);
  DataBlockCache::setDataBlock(hashkey, dblock);

}

```

* example datablock/chunk reader


```cpp
void gpu_data_reader(datablock_ptr_t dblock){

  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(dblock, allocator);
  OrkAssert(chunkreader._chunkfiletype == "mygpuchunkformat");
  if (chunkreader.IsOk()) {
    auto hdrstream = chunkreader.GetStream("header");
    auto gpustream = chunkreader.GetStream("gpudata");
    
    uint64_t key, offset, length, count;

    hdrstream->GetItem<uint64_t>(count);
	
    std::vector<uint8_t> gpu_data;
    for( size_t i=0; i<count; i++ ){
      hdrstream->GetItem<uint64_t>(key);
      switch(key){
 	case  "gpudataoffset"_crcu:
          hdrstream->GetItem<uint64_t>(offset);
	  break;
	case  "gpudatalength"_crcu:{
	  hdrstream->GetItem<uint64_t>(length);
	  auto gpudata = (const uint8_t*) geostream->GetDataAt(offset);
			// we now have length and offset of gpu data item
			// send it to the GPU !
	  break;
        }
      }
    }
  }
}
```
 
