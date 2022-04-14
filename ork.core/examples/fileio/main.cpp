ork::File outputfile(Filename, ork::EFM_WRITE);
outputfile.Write(datablock->data(), datablock->length());

if (FileEnv::GetRef().DoesFileExist(path)) {
    ork::File inputfile(path, ork::EFM_READ);
    size_t length = 0;
    inputfile.GetLength(length);
    rval       = std::make_shared<DataBlock>();
    void* dest = rval->allocateBlock(length);
    inputfile.Read(dest, length);
    inputfile.Close();
  }
