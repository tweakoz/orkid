#pragma once

#include <ork/file/path.h>
#include <memory>
#include <stdlib.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>

namespace ork::object {

template <typename T>
std::shared_ptr<T> deserialize(file::Path path){

	std::shared_ptr<T> out_object;

	if (FileEnv::GetRef().DoesFileExist(path)) {
    ork::File inputfile(path, ork::EFM_READ);
    size_t length = 0;
    inputfile.GetLength(length);
    auto buffer = (char*) malloc(length+1);
    inputfile.Read(buffer, length);
    buffer[length] = 0;
    inputfile.Close();
    object_ptr_t instance_out;
    auto deser = std::make_shared<reflect::serdes::JsonDeserializer>(buffer);
    deser->deserializeTop(instance_out);
    out_object = std::dynamic_pointer_cast<T>(instance_out);
    deser = nullptr;
    free(buffer);
  }

  return out_object;

}

} //namespace ork::object {
