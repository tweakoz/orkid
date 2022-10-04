////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>
#include <ork/kernel/netpacket_dyn.inl>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/quaternion.h>
#include <ork/math/plane.h>

////////////////////////////////////////////////////////////////

namespace ork {
using fvec2vector_t        = std::vector<fvec2>;
using fvec3vector_t        = std::vector<fvec3>;
using fvec4vector_t        = std::vector<fvec4>;
using fquatvector_t        = std::vector<fquat>;
using fmtx3vector_t        = std::vector<fmtx3>;
using fmtx4vector_t        = std::vector<fmtx4>;
using fplane3vector_t      = std::vector<fplane3>;
using bytes_t              = std::vector<uint8_t>;
using int16vector_t        = std::vector<int16_t>;
using int32vector_t        = std::vector<int32_t>;
using int64vector_t        = std::vector<int64_t>;
using float_vect_t         = std::vector<float>;
using double_vect_t         = std::vector<double>;
}

namespace ork::net::serdes {

struct Serializer;
struct Deserializer;

using key_t                = std::string;
using val_t                = ork::svar64_t;
using vlist_t         		 = std::vector<val_t>;
using kvmap_t              = std::map<key_t, val_t>;
using fixuparray_t = vlist_t;

///////////////////////////////////////////////////////////
inline const val_t& item_from_kvmap(const kvmap_t & the_map, const std::string& key){
  static const val_t KNOTFOUND;
  auto it = the_map.find(key);
  if(it!=the_map.end()){
    return it->second;
  }
  return KNOTFOUND;
}
inline bool item_in_kvmap(const kvmap_t & the_map, const std::string& key){
  auto it = the_map.find(key);
  return (it!=the_map.end());
}
template <typename T> 
inline  const T& typed_item_from_kvmap(const kvmap_t & the_map, const std::string& key){
  const val_t& the_val = item_from_kvmap(the_map,key);
  return the_val.get<T>();
}
///////////////////////////////////////////////////////////


using on_fixup_t = std::function<void(val_t& value_created)>;

using serfn_t = std::function<void(Serializer* ser, msgpacketbase_ref_t, const val_t& inp_value)>;
using deserfn_t = std::function<void(Deserializer* ser, MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& fixupfn )>;

struct Serializer {

	Serializer(bool std_types=true);

	void serialize(msgpacketbase_ref_t msg, const val_t& value);

	template <typename T>
	void registerType(serfn_t serfn){
		auto tinfo = & typeid(T);
		const char* tname = tinfo->name();
		_typehandlers[tname] = serfn;
	}

	std::unordered_map<std::string,serfn_t> _typehandlers;
};

struct Deserializer {
	Deserializer(bool std_types=true);
	void deserialize(MessagePacketIteratorBase& iter, val_t& out_value, const on_fixup_t& = [](val_t&){});

	void registerType(std::string username, deserfn_t deserfn){
		_typehandlers[username] = deserfn;
	}

	std::unordered_map<std::string,deserfn_t> _typehandlers;
};

} //namespace ork::net::serdes {
