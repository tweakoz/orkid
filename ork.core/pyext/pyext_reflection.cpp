///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/hotkey.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork {
//using namespace rtti;
///////////////////////////////////////////////////////////////////////////////
using class_pyptr_t               = ork::python::unmanaged_ptr<rtti::Class>;
///////////////////////////////////////////////////////////////////////////////
void pyinit_reflection(py::module& module_core) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
    auto class_type_t = py::class_<class_pyptr_t>(module_core, "Class") //
      .def_property_readonly("name", [](class_pyptr_t clazz) -> std::string {
        return clazz->Name().c_str();
      });
  type_codec->registerStdCodec<class_pyptr_t>(class_type_t);
  /////////////////////////////////////////////////////////////////////////////////
    auto icastable_type_t = py::class_<rtti::ICastable,rtti::castable_ptr_t>(module_core, "ICastable") //
      .def_property_readonly("clazz", [](rtti::castable_ptr_t castable) -> class_pyptr_t {
        return castable->GetClass(); 
      });
  type_codec->registerStdCodec<rtti::castable_ptr_t>(icastable_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  struct PropertiesProxy {
    PropertiesProxy(object_ptr_t obj,ork::python::typecodec_ptr_t codec)
        : _object(obj)
        , _codec(codec) {
    }
    py::object decodeProperty(const std::string& key) const {
      auto clazz = _object->GetClass();
      auto objclazz = dynamic_cast<object::ObjectClass*>(clazz);
      auto& desc = objclazz->Description();
      for( auto pitem : desc.properties() ){
        auto refprop = pitem.second;
        auto propname = refprop->_name;
        if(propname==key){
          using ityped_int = reflect::ITyped<int>;
          using ityped_float = reflect::ITyped<float>;
          using ityped_string = reflect::ITyped<std::string>;
          using ityped_int_array = reflect::ITypedArray<int>;
          using ityped_float_array = reflect::ITypedArray<float>;
          varmap::var_t variant;
          if( auto as_int = dynamic_cast<ityped_int*>(refprop) ){
            int intvalue = 0;
            as_int->get(intvalue,_object);
            variant.set<int>(intvalue);
            return _codec->encode(variant);
          }
          else if( auto as_float = dynamic_cast<ityped_float*>(refprop) ){
            float floatvalue = 0;
            as_float->get(floatvalue,_object);
            variant.set<float>(floatvalue);
            return _codec->encode(variant);
          }
          else if( auto as_str = dynamic_cast<ityped_string*>(refprop) ){
            std::string strvalue;
            as_str->get(strvalue,_object);
            variant.set<std::string>(strvalue);
            return _codec->encode(variant);
          }
          else if( auto as_intarray = dynamic_cast<ityped_int_array*>(refprop) ){
            return python::IntArrayPropertyCodec::encode( _object,//
                                                          as_intarray, //
                                                          _codec);
          }
          else if( auto as_floatarray = dynamic_cast<ityped_float_array*>(refprop) ){
            return python::FloatArrayPropertyCodec::encode( _object,//
                                                            as_floatarray, //
                                                            _codec);
          }
          else{
            printf( "reflection class<%s> prop<%s> unhandled type>\n", clazz->Name().c_str(), propname.c_str());
            OrkAssert(false);
          }
        }
      }
      return py::none();
    }
    object_ptr_t _object;
    ork::python::typecodec_ptr_t _codec;
  };
  using propsproxy_ptr_t = std::shared_ptr<PropertiesProxy>;
  auto propsproxy_type   =                                                        //
      py::class_<PropertiesProxy, propsproxy_ptr_t>(module_core, "PropertiesProxy") //
          .def(
              "__getattr__",                                                           //
              [](propsproxy_ptr_t proxy, const std::string& key) -> py::object { //
                return proxy->decodeProperty(key);
              })
          .def(
              "__setattr__",                                                           //
              [](propsproxy_ptr_t proxy, const std::string& key, py::object value) { //
                auto obj = proxy->_object;
                auto clazz = obj->GetClass();
                auto objclazz = dynamic_cast<object::ObjectClass*>(clazz);
                auto& desc = objclazz->Description();
                for( auto pitem : desc.properties() ){
                  auto refprop = pitem.second;
                  auto propname = refprop->_name;
                  if(propname==key){
                    auto variant = proxy->_codec->decode(value);
                    using ityped_int = reflect::ITyped<int>;
                    using ityped_float = reflect::ITyped<float>;
                    using ityped_string = reflect::ITyped<std::string>;
                    if( auto as_int = dynamic_cast<ityped_int*>(refprop) ){
                      as_int->set(variant.get<int>(),obj);
                      return;
                    }
                    else if( auto as_float = dynamic_cast<ityped_float*>(refprop) ){
                      as_float->set(variant.get<float>(),obj);
                      return;
                    }
                    else if( auto as_str = dynamic_cast<ityped_string*>(refprop) ){
                      as_str->set(variant.get<std::string>(),obj);
                      return;
                    }
                    else{
                      printf( "reflection class<%s> prop<%s> unhandled type>\n", clazz->Name().c_str(), propname.c_str());
                      OrkAssert(false);
                    }
                                        
                  }
                }
                OrkAssert(false);
              })
          .def_property_readonly("dict", [](propsproxy_ptr_t proxy) -> py::dict {
            py::dict rval;
            auto obj = proxy->_object;
            auto clazz = obj->GetClass();
            auto objclazz = dynamic_cast<object::ObjectClass*>(clazz);
            auto& desc = objclazz->Description();
            for( auto p : desc.properties() ){
              auto key = p.second->_name;
              rval[proxy->_codec->encode(key)] = proxy->decodeProperty(key);
            }
            return rval;
          });
  type_codec->registerStdCodec<propsproxy_ptr_t>(propsproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto objtype_t = py::class_<Object,rtti::ICastable,object_ptr_t>(module_core, "Object")
    .def_static("deserializeJson", [](std::string json) -> object_ptr_t {
        reflect::serdes::JsonDeserializer deser(json.c_str());
        object_ptr_t instance_out;
        deser.deserializeTop(instance_out);
        return instance_out;
    })
    .def("serializeJson", [](object_ptr_t obj) -> std::string {
        reflect::serdes::JsonSerializer ser;
        auto topnode    = ser.serializeRoot(obj);
        auto resultdata = ser.output();
        return resultdata.c_str();
    })
    .def_property_readonly("properties", [type_codec](object_ptr_t obj) -> propsproxy_ptr_t {
      return std::make_shared<PropertiesProxy>(obj,type_codec);
    });
  type_codec->registerStdCodec<object_ptr_t>(objtype_t);
  /////////////////////////////////////////////////////////////////////////////////
  using hotkey_ptr_t = std::shared_ptr<HotKey>;
  auto hkey_type =                                                              //
      py::class_<HotKey,Object,hotkey_ptr_t>(module_core, "HotKey") //
          .def(py::init<>());
  type_codec->registerStdCodec<hotkey_ptr_t>(hkey_type);
  /////////////////////////////////////////////////////////////////////////////////
  using hotkeyconfig_ptr_t = std::shared_ptr<HotKeyConfiguration>;
  auto hkeycfg_type =                                                              //
      py::class_<HotKeyConfiguration,Object,hotkeyconfig_ptr_t>(module_core, "HotKeyConfiguration") //
          .def(py::init<>())
          .def("createHotKey", [](HotKeyConfiguration& hkc, std::string actionname) -> hotkey_ptr_t {
            auto hk = std::make_shared<HotKey>();
            hkc._hotkeys.AddSorted(actionname,hk);
            return hk;
          });
  type_codec->registerStdCodec<hotkeyconfig_ptr_t>(hkeycfg_type);
  /////////////////////////////////////////////////////////////////////////////////


}

///////////////////////////////////////////////////////////////////////////////
} 
