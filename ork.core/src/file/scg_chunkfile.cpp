////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/kernel.h>
#include <ork/file/scg_chunkfile.h>
#include <ork/file/efileenum.h>
#include <ork/util/hexdump.inl>

namespace ork::scg {

///////////////////////////////////////////////////////////////////////////////

chunkdataiter_ptr_t ChunkData::iterator() const{
	return std::make_shared<ChunkDataIterator>(this);
}

size_t ChunkData::length() const{
	return _data.size();
}

ChunkDataIterator::ChunkDataIterator(const ChunkData* cdata) : _chunkdata(cdata) {}

///////////////////////////////////////////////////////////////////////////////

void ChunkReaderFile::seek(size_t seekpos) {
  EFileErrCode eFileErr = _ork_file->SeekFromStart(seekpos);
  OrkAssert(EFEC_FILE_OK == eFileErr);
  _offset = seekpos;
}
void ChunkReaderFile::read(void* pwhere, size_t ihowmany) {
  OrkAssert(_ork_file->miUserPos == _offset);
  EFileErrCode eFileErr = _ork_file->Read(pwhere, ihowmany);
  OrkAssert(EFEC_FILE_OK == eFileErr);
  _offset += ihowmany;
}

///////////////////////////////////////////////////////////////////////////////

ChunkSubReader::ChunkSubReader(chunkdata_ptr_t chdata) //
  : _chunkdata(chdata) { //
}

void ChunkSubReader::seek(size_t seekpos) {
  _offset = seekpos;
}
void ChunkSubReader::read(void* pwhere, size_t howmany) {
  memcpy(pwhere, _chunkdata->_data.data() + _offset, howmany);
  _offset += howmany;	
}

///////////////////////////////////////////////////////////////////////////////

ChunkFile::ChunkFile(file::Path path){
  auto reader = std::make_shared<ChunkReaderFile>();
  reader->_ork_file = std::make_shared<File>(path, EFileMode::EFM_READ);

  _top_reader = reader;
  chunkfileID = _top_reader->readItem<uint32_t>();
  if (chunkfileID != chunk_genchunkID("CHKF")) {
    OrkAssert(false); // not an scg chunkfile
  }
  filetypeID = _top_reader->readItem<uint32_t>();
  _top_chunkgroup = _loadSubHeader(8);
}

///////////////////////////////////////////////////////////////////////////////

chunkdata_ptr_t ChunkGroup::load( chunkheader_ptr_t chunkhdr ){
  uint32_t offset = chunkhdr->offset + _offsetbase;
  uint32_t len    = chunkhdr->chunklen;
  auto data       = std::make_shared<ChunkData>();
  data->_data.resize(len);
  printf( "LOAD: seek<%08x> read<%08x>\n", offset, len );
  _reader->seek(offset);
  _reader->read(data->_data.data(), len);
  hexdumpbytes(data->_data.data(), 40 );
  data->_versionCode = chunk_genchunkIDSTRING(chunkhdr->version);
  return data;
}

///////////////////////////////////////////////////////////////////////////////

chunkgroup_ptr_t ChunkFile::_loadSubHeader(size_t seekoffset) {
  auto chgrp = std::make_shared<ChunkGroup>();
  chgrp->_name = FormatString("TopChunkGroup[offset<%08x>]", seekoffset);
  chgrp->_reader = _top_reader;
  chgrp->_offsetbase = seekoffset;

  _top_reader->seek(seekoffset);
  chgrp->numchunks = _top_reader->readItem<uint32_t>();

  for (size_t i = 0; i < chgrp->numchunks; i++) {
	auto ch = std::make_shared<ChunkHeader>();
	_top_reader->read((void*) ch.get(), sizeof(ChunkHeader));
	chgrp->_chunk_headers.push_back(ch);
  }
  return chgrp;
}
///////////////////////////////////////////////////////////////////////////////

chunkgroup_ptr_t ChunkFile::_loadSubHeader(chunkdata_ptr_t parent) {

  hexdumpbytes(parent->_data.data(), 40 );
  auto chgrp = std::make_shared<ChunkGroup>();
  chgrp->_name = FormatString("ChildChunkGroup[cdata<%p>]", (void*) parent.get());
  chgrp->_reader = std::make_shared<ChunkSubReader>(parent);
  chgrp->numchunks = chgrp->_reader->readItem<uint32_t>();
  printf( "CH.NUMCHUNKS<%d>\n", chgrp->numchunks );


  for (size_t i = 0; i < chgrp->numchunks; i++) {
	auto ch = std::make_shared<ChunkHeader>();
	(*ch) = chgrp->_reader->readItem<ChunkHeader>();
	chgrp->_chunk_headers.push_back(ch);
  }
  return chgrp;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<chunkheader_ptr_t> ChunkGroup::chunksOfType( std::string chunkID ) const{
	std::vector<chunkheader_ptr_t> rval;
	auto cid_int = chunk_genchunkID(chunkID.c_str());
	for( auto item : _chunk_headers ){
		if( cid_int == item->chunktypeID ){
			rval.push_back(item);
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

chunkheader_ptr_t ChunkGroup::getChunkOfType( std::string chunkID ) const{
	auto chunks = chunksOfType(chunkID);
	chunkheader_ptr_t rval;
	switch(chunks.size()){
		case 1:
	  	  rval = chunks[0];
		  break;
		case 0:
		  break;
		default:
		  // ambiguous which chunk to return
		  OrkAssert(false);
		  break;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ChunkGroup::dump_headers(std::string headers_name) {

  printf("//////////////////////////////////////////////////////\n");
  printf("// headers<%s> Listing chunks\n", headers_name.c_str() );
  printf("//////////////////////////////////////////////////////\n");

  for (int i = 0; (i < _chunk_headers.size()); i++) {
    chunkheader_ptr_t hdr = _chunk_headers[i];
    uint32_t chunkTID      = hdr->chunktypeID;
    std::string chunkNAME = chunk_genchunkIDSTRING(hdr->chunktypeID);
    std::string chunkVERS = chunk_genchunkIDSTRING(hdr->version);

    printf("// chunk<%d> : CTID<%08x:%s> inst<%08x> vers<%s> offs %08x len %08x\n", i, chunkTID, chunkNAME.c_str(), hdr->chunkID, chunkVERS.c_str(), hdr->offset, hdr->chunklen);
  }
  printf("//////////////////////////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

uint32_t chunk_genchunkID(const char* chptr) {
  uint32_t rval = 0xffffffff;
  if (chptr == 0) { // ASSERT( 0 );
  }
  if (strlen(chptr) != 4) { // ASSERT( 0 );
  }
  rval = chptr[3] << 24;
  rval |= chptr[2] << 16;
  rval |= chptr[1] << 8;
  rval |= chptr[0];
  // printf("getChunkID %s \n",chptr);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

const char* chunk_genchunkIDSTRING(uint32_t chid) {
  static char buf[250];
  static int idx = 0;
  char* ch       = &buf[idx];
  idx += 5;
  idx %= 250;
  ch[0] = (char)(chid)&0x000000ff;
  ch[1] = (char)(chid >> 8) & 0x000000ff;
  ch[2] = (char)(chid >> 16) & 0x000000ff;
  ch[3] = (char)(chid >> 24) & 0x000000ff;
  ch[4] = 0;

  struct FixAscii {
    static char doit(char in) {
      char out = ' ';

      bool bok1 = (in >= 'a') && (in <= 'z');
      bool bok2 = (in >= 'A') && (in <= 'Z');
      bool bok3 = (in >= '0') && (in <= '9');
      bool bok4 = (in >= ')') && (in <= '!');
      bool bok5 = (in == '_') || (in == ' ');

      bool bok = bok1 || bok2 || bok3 || bok4 || bok5;

      return bok ? in : out;
    }
  };

  ch[0] = FixAscii::doit(ch[0]);
  ch[1] = FixAscii::doit(ch[1]);
  ch[2] = FixAscii::doit(ch[2]);
  ch[3] = FixAscii::doit(ch[3]);

  return ch;
}

///////////////////////////////////////////////////////////////////////////////
// load a particular chunk ( mallocs own memory )

/*chunkdata_ptr_t chunk_LoadChunk(chunkgroup_ptr_t chgrp, ChunkHandle chunkIDX) {
  struct chunkfile_ptr_t chfil             = chgrp->chfil;
  chunkheader_ptr_t hdr                = chunk_GetChunkInfo(chgrp, chunkIDX);
  uint32_t offset                      = hdr->offset + chgrp->offsetbase;
  uint32_t len                         = hdr->chunklen;
  auto data                    = std::make_shared<ChunkData>();
  data->_data.resize(len);
  //chgrp->chunk_malloced_addr[chunkIDX] = data;
  // printf( "//////////////////////////////////////////////////////\n" );
  // printf( "LoadChunk chunkIDX %08x offset %08x len %08x TID < %s >\n", chunkIDX, offset, len, genchunkIDSTRING( hdr->chunktypeID
  // ) ); printf( "//////////////////////////////////////////////////////\n" );
  chfil->seek(offset);
  chfil->read(data->_data.data(), len);
  return data;
}*/

///////////////////////////////////////////////////////////////////////////////
// get me first ? particular chunktypeID with matching chunkID
/*
ChunkHandle chunk_ChunkOfType(chunkgroup_ptr_t chgrp, uint32_t chunktypeID, uint32_t chunkID) {
  uint32_t nchunks     = chgrp->numchunks;
  ChunkHandle chunkIDX = 0xffffffff;
  uint32_t i;

  printf("//////////////////////////////////////////////////////\n");
  printf(
      "searching for chunktype < %08x : %08x > < %s : %s >\n",
      chunktypeID,
      chunkID,
      chunk_genchunkIDSTRING(chunktypeID),
      chunk_genchunkIDSTRING(chunkID));

  for (i = 0; ((i < nchunks) && (chunkIDX == 0xffffffff)); i++) {
    chunkheader_ptr_t hdr = chunk_GetChunkInfo(chgrp, i);
    printf(
        "comparing to idx %d : TID < %08x : %s > CID < %08x > offs %08x len %08x\n",
        i,
        hdr->chunktypeID,
        chunk_genchunkIDSTRING(hdr->chunktypeID),
        hdr->chunkID,
        hdr->offset,
        hdr->chunklen);
    if ((chunktypeID == hdr->chunktypeID) && (chunkID == hdr->chunkID))
      chunkIDX = i;
  }
  printf("found %08x\n", chunkIDX);
  printf("//////////////////////////////////////////////////////\n");
  return chunkIDX;
}*/

///////////////////////////////////////////////////////////////////////////////
// get me first ? particular chunktypeID with matching chunkID


///////////////////////////////////////////////////////////////////////////////
// get me the number of chunks of a particular chunktype
/*
uint32_t chunk_NumOfChunkType(chunkgroup_ptr_t chgrp, uint32_t chunktypeID) {
  uint32_t i;
  uint32_t nchunks;
  uint32_t nmatch;
  chunkheader_ptr_t hdr;
  nchunks = chgrp->numchunks;
  nmatch  = 0;
  for (i = 0; i < nchunks; i++) {
    hdr = chunk_GetChunkInfo(chgrp, i);
    if (chunktypeID == hdr->chunktypeID)
      nmatch++;
  }
  return nmatch;
}

///////////////////////////////////////////////////////////////////////////////
// get me the nth instance of a particular chunktype

ChunkHandle chunk_NthChunkOfType(chunkgroup_ptr_t chgrp, uint32_t chunktypeID, uint32_t instancenum) {
  uint32_t i;
  uint32_t nchunks;
  uint32_t nmatch;
  ChunkHandle chunkIDX;
  chunkheader_ptr_t hdr;
  nchunks  = chgrp->numchunks;
  nmatch   = 0;
  chunkIDX = 0xffffffff;
  for (i = 0; ((i < nchunks) && (chunkIDX == 0xffffffff)); i++) {
    hdr = chunk_GetChunkInfo(chgrp, i);
    if (chunktypeID == hdr->chunktypeID) {
      if (nmatch == instancenum)
        chunkIDX = i;
      nmatch++;
    }
  }
  return chunkIDX;
}
*/

/* old writers
Cchunkgroup::Cchunkgroup()
    : chunkgroup_length(0) {
}

// namespace Orkid {

CChunk::CChunk()
    : miReadIndex(0)
    , chunktypeID(0)
    , offset_base(0)
    , offset_current(0) {
}

///////////////////////////////////////////////////////////////////////////////
// initialize a CChunk from a loaded chunk

CChunk::CChunk(chunkgroup_ptr_t chgrp, ChunkHandle chunkIDX)
    : miReadIndex(0)
    , chunktypeID(0)
    , offset_base(0)
    , offset_current(0)
    , version_code(0) {
  struct chunkfile_ptr_t chfil = chgrp->chfil;
  chunkheader_ptr_t hdr    = chunk_GetChunkInfo(chgrp, chunkIDX);
  uint32_t offset          = hdr->offset + chgrp->offsetbase;
  int ilen                 = hdr->chunklen;
  const char* pdata        = (const char*)chunk_malloc(ilen);

  version_code = hdr->version;

  chunk_fseek(chfil->_ork_file, offset);
  chunk_fread(chfil->_ork_file, (void*)pdata, ilen);

  for (int i = 0; i < ilen; i++) {
    this->data_vect.push_back(pdata[i]);
  }
}

///////////////////////////////////////////////////////////////////////////////

// Cchunkgroup::Cchunkgroup()
//{
//	chunkgroup_length = 0;
// }

///////////////////////////////////////////////////////////////////////////////

Cchunkgroup::~Cchunkgroup() {
}

///////////////////////////////////////////////////////////////////////////////

void CChunk::data_dump(void) {
  printf("/////////////////////////////////////\n");

  uint32_t dlen = data_vect.size();
  uint32_t col  = 0;

  //	printf( "chunk %08x %s data dump length = %08x\n", this, genchunkIDSTRING( chunktypeID ), dlen );

  for (uint32_t i = 0; i < dlen; i++) {
    U8 byte = data_vect[i];

    if (col == 0)
      printf("offs %08x	", i);

    printf("%02x", byte);

    col++;

    if ((col % 4) == 0)
      printf(" ");

    if ((col % 32) == 0)

    {
      col = 0;
      printf("\n");
    }
  }

  printf("\n/////////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

uint32_t CChunk::AddData(const char* data, int len) {
  uint32_t retval = offset_current;

  OrkAssert(0 != data);

  for (int i = 0; i < len; i++) {
    char ch = data[i];
    data_vect.push_back(ch);
  }

  offset_current += len;
  return (retval);
}

///////////////////////////////////////////////////////////////////////////////

void CChunk::GetData(char* data, int len) const {
  OrkAssert(0 != data);

  for (int i = 0; i < len; i++) {
    char ch = data_vect[i + miReadIndex];
    data[i] = ch;
  }

  miReadIndex += len;
}

///////////////////////////////////////////////////////////////////////////////

void CChunk::data_pad(void) {
  int clen    = data_vect.size();
  int npad    = (4 - (clen % 4)) % 4;
  char pad[4] = {0, 0, 0, 0};
  AddData(pad, npad);

  // printf( "length %08x padded %d bytes\n", clen, npad );
}

///////////////////////////////////////////////////////////////////////////////

void CChunk::AddSubChunkData(chunkgroup_ptr_t chgrp) {
  // printf( "begin inline data\n" );

  chgrp->calc_offsets();

  uint32_t nchunks = chgrp->chunk_vect.size();

  // printf( "nchunks %d\n", nchunks );

  AddData((char*)&nchunks, sizeof(uint32_t));

  // write chunkheaders
  for (uint32_t c = 0; c < nchunks; c++) {
    ChunkHeader chunkheader;
    auto chunk = chgrp->chunk_vect[c];

    chunkheader.chunktypeID = chunk->chunktypeID;
    chunkheader.chunkID     = chunk->chunkID;
    chunkheader.chunklen    = chunk->data_vect.size();
    chunkheader.offset      = chunk->offset_base;
    chunkheader.version     = chunk->version_code;

    AddData((char*)&chunkheader, sizeof(ChunkHeader));
  }

  // write chunkdata
  for (uint32_t c = 0; c < nchunks; c++) {
    auto chunk      = chgrp->chunk_vect[c];
    uint32_t chsize = chunk->data_vect.size();

    // printf( "inline ch %d size %08x\n", c, chsize );

    for (uint32_t d = 0; d < chsize; d++) {
      char ch = chunk->data_vect[d];
      AddData(&ch, 1);
    }
  }

  // printf( "end inline data\n" );
}

///////////////////////////////////////////////////////////////////////////////

void CChunk::WriteData(FILE* fout) const {
  uint32_t nbytes = data_vect.size();

  for (uint32_t i = 0; i < nbytes; i++) {
    char ch = data_vect[i];
    fwrite((char*)&ch, 1, 1, fout);
  }
}

///////////////////////////////////////////////////////////////////////////////

chunk_ptr_t Cchunkgroup::newchunk(char* chtID, char* chID, char* verID) {
  uint32_t tID = chunk_genchunkID(chtID);
  uint32_t ID  = chunk_genchunkID(chID);
  uint32_t vID = chunk_genchunkID(verID);

  auto chunk = std::make_shared<CChunk>();

  chunk->chunktypeID  = tID;
  chunk->chunkID      = ID;
  chunk->version_code = vID;

  chunk_vect.push_back(chunk);

  return chunk;
}

///////////////////////////////////////////////////////////////////////////////

chunk_ptr_t Cchunkgroup::newchunk(char* chtID, uint32_t chID, char* verID) {
  uint32_t tID = chunk_genchunkID(chtID);
  uint32_t vID = chunk_genchunkID(verID);

  auto chunk = std::make_shared<CChunk>();

  chunk->chunktypeID  = tID;
  chunk->chunkID      = chID;
  chunk->version_code = vID;

  chunk_vect.push_back(chunk);

  return chunk;
}

///////////////////////////////////////////////////////////////////////////////

void Cchunkgroup::calc_offsets(void) {
  uint32_t nchunks = chunk_vect.size();

  chunkgroup_length = 4 + (nchunks * sizeof(ChunkHeader)); //	chunk headers

  // printf( "nchunks %d offbase %08x\n", nchunks, chunkgroup_length );

  for (uint32_t c = 0; c < nchunks; c++) {
    auto chunk         = chunk_vect[c];
    chunk->offset_base = chunkgroup_length;
    chunkgroup_length += chunk->data_vect.size();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Cchunkgroup::data_write(FILE* fout) {
  ChunkHeader chunkheader;

  uint32_t nchunks = chunk_vect.size();

  // write chunkgroup hdr (numchunks)
  fwrite((char*)&nchunks, 1, sizeof(uint32_t), fout);

  // write chunkheaders
  uint32_t c;
  for (c = 0; c < nchunks; c++) {
    auto chunk = chunk_vect[c];

    chunkheader.chunktypeID = chunk->chunktypeID;
    chunkheader.chunkID     = chunk->chunkID;
    chunkheader.chunklen    = chunk->data_vect.size();
    chunkheader.offset      = chunk->offset_base;
    chunkheader.version     = chunk->version_code;

    fwrite((void*)&chunkheader, 1, sizeof(ChunkHeader), fout);
  }

  // write chunkdata
  for (c = 0; c < nchunks; c++) {
    auto chunk = chunk_vect[c];
    chunk->WriteData(fout);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Cchunkgroup::write_chunk_file(std::string fname, uint32_t filetypeID) {
  uint32_t chunkfileID = chunk_genchunkID("CHKF");

  calc_offsets();

  FILE* fout = fopen(fname.c_str(), "wb");

  if (fout == 0) {
    OrkAssertI(false, FormatString("cannot open file for write %s\n", fname.c_str()).c_str());
    // fl_alert( "file cannot be opened for write, check if used or locked! (file not saved)" );

  } else {
    fwrite((void*)&chunkfileID, 1, sizeof(uint32_t), fout);
    fwrite((void*)&filetypeID, 1, sizeof(uint32_t), fout);

    data_write(fout);

    fclose(fout);
  }
}

//};
*/
} // namespace ork::scg
