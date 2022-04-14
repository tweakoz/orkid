////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/ecs/datatable.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>

using namespace ::ork;

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
bool DataKey::valid() const { 
	return _encoded.Isset();
}
///////////////////////////////////////////////////////////////////////////////
bool DataKey::operator == (const DataKey& rhs) const {
	return _encoded == rhs._encoded;
}
///////////////////////////////////////////////////////////////////////////////
bool DataVar::valid() const { 
	return _encoded.Isset();
}
///////////////////////////////////////////////////////////////////////////////
bool DataVar::operator == (const DataVar& rhs) const {
	return _encoded == rhs._encoded;
}
///////////////////////////////////////////////////////////////////////////////
DataVar DataTable::find(const DataKey& key) const {
	for( const auto& item : _items ){
		if(item._key == key){
			return item._val;
		}
	}
	DataVar rval;
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
svar64_t& DataTable::operator[](const DataKey& key) {
	for( auto& item : _items ){
		if(item._key == key){
			return item._val._encoded;
		}
	}
	DataKvPair new_item{
		._key = key,
		._val = DataVar()
	};
	_items.push_back(new_item);
	return _items.rbegin()->_val._encoded;
}
///////////////////////////////////////////////////////////////////////////////
svar64_t& DataTable::operator[](const CrcString& key) {
	DataKey k;
	k._encoded.set<CrcString>(key);
	return operator[](k);
}
///////////////////////////////////////////////////////////////////////////////
const svar64_t& DataTable::operator[](const DataKey& key) const {
	for( auto& item : _items ){
		if(item._key == key){
			return item._val._encoded;
		}
	}
	static const svar64_t _NOTFOUND;
	return _NOTFOUND;
}
///////////////////////////////////////////////////////////////////////////////
const svar64_t& DataTable::operator[](const CrcString& key) const {
	DataKey k;
	k._encoded.set<CrcString>(key);
	return operator[](k);
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
