////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/ITypedArray.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <ork/util/logger.h>

#include <ork/orkprotos.h>
#include <cstring>

using namespace rapidjson;

namespace ork::reflect::serdes {
static logchannel_ptr_t logchan_ds = logger()->createChannel("reflection.json.deser",fvec3(0.9,1,0.9), false);

//////////////////////////////////////////////////////////////////////////////

struct JsonLeafNode {
  JsonLeafNode(const rapidjson::Value& jn)
      : _jsonnode(jn) {
  }
  const rapidjson::Value& _jsonnode;
};
struct JsonObjectNode {
  JsonObjectNode(const rapidjson::Value& jn)
      : _jsonobjectnode(jn) {
  }
  const rapidjson::Value& _jsonobjectnode;
  Value::ConstMemberIterator _iterator;
};
struct JsonArrayNode {
  JsonArrayNode(const rapidjson::Value& jn)
      : _jsonarray(jn) {
  }
  const rapidjson::Value& _jsonarray;
  Value::ConstValueIterator _iterator;
};

//////////////////////////////////////////////////////////////////////////////

JsonDeserializer::JsonDeserializer(const std::string& jsondata)
    : _document() {
  //_allocator = &_document.GetAllocator();
  _document.Parse(jsondata.c_str());
  bool is_object = _document.IsObject();
  bool has_top   = _document.HasMember("root");
  OrkAssert(is_object);
  OrkAssert(has_top);
  _property_stack.push(nullptr);
}

//////////////////////////////////////////////////////////////////////////////

node_ptr_t JsonDeserializer::pushNode(std::string named, NodeType type) {
  node_ptr_t n     = std::make_shared<Node>();
  n->_name         = named;
  n->_deserializer = this;
  n->_type         = type;
  switch (type) {
    case NodeType::MAP:
      OrkAssert(false);
      break;
    case NodeType::MAP_ELEMENT_LEAF:
    case NodeType::MAP_ELEMENT_OBJECT:
      n->_impl.makeShared<JsonObjectNode>(rapidjson::Value());
      break;
    case NodeType::ARRAY:
      OrkAssert(false);
      break;
    case NodeType::ARRAY_ELEMENT_LEAF:
    case NodeType::ARRAY_ELEMENT_OBJECT:
      n->_impl.makeShared<JsonArrayNode>(rapidjson::Value());
      break;
    case NodeType::OBJECT:
      n->_impl.makeShared<JsonObjectNode>(rapidjson::Value());
      break;
    case NodeType::LEAF:
      n->_impl.makeShared<JsonLeafNode>(rapidjson::Value());
      break;
    case NodeType::UNKNOWN:
    case NodeType::PROPERTIES:
      OrkAssert(false);
      break;
  }
  _nodestack.push(n);
  return n;
}
void JsonDeserializer::popNode() {
  _nodestack.pop();
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeTop(object_ptr_t& instance_out) {
  const auto& rootnode        = _document["root"];
  auto topnode                = this->pushNode("root", NodeType::OBJECT);
  auto child_node             = _parseSubNode(topnode, rootnode);
  instance_out                = child_node->_deser_instance;
  auto instance_out_classname = instance_out->GetClass()->Name();
  std::string uuids           = boost::uuids::to_string(instance_out->_uuid);

  if (0)
    logchan_ds->log(
        "top instance<%p> class<%s> uuid<%s>\n", //
        (void*) instance_out.get(),
        instance_out_classname.c_str(),
        uuids.c_str());

  this->popNode();
}

//////////////////////////////////////////////////////////////////////////////

node_ptr_t JsonDeserializer::deserializeObject(node_ptr_t parnode) {
  auto topnode = pushNode("object", NodeType::OBJECT);

  if (parnode->_impl.isShared<JsonLeafNode>()) {
    OrkAssert(parnode->_value.get<std::string>() == "nil");
    return nullptr;
  } else {
    auto implnode       = parnode->_impl.getShared<JsonObjectNode>();
    const auto& jsonval = implnode->_jsonobjectnode;
    OrkAssert(jsonval.IsObject());
    OrkAssert(jsonval.HasMember("object") or jsonval.HasMember("object-ref"));
    auto child_node   = _parseSubNode(topnode, jsonval);
    auto instance_out = child_node->_deser_instance;
    popNode();
    return child_node;
  }
}

//////////////////////////////////////////////////////////////////////////////

serdes::node_ptr_t JsonDeserializer::_parseSubNode(
    serdes::node_ptr_t parentnode, //
    const rapidjson::Value& subvalue) {

  auto child_node             = pushNode("", NodeType::OBJECT);
  child_node->_parent         = parentnode;
  child_node->_property       = parentnode->_property;
  child_node->_deser_instance = parentnode->_deser_instance;

  switch (subvalue.GetType()) {
    case rapidjson::kObjectType: {
      //////////////////////////////////////////////////////////////////////////
      if (subvalue.HasMember("object")) { // inline object
      //////////////////////////////////////////////////////////////////////////
        const auto& jsonobjnode = subvalue["object"];
        switch (jsonobjnode.GetType()) {
          case rapidjson::kObjectType: {

            auto implnode               = child_node->_impl.makeShared<JsonObjectNode>(jsonobjnode);
            auto instance_out           = _parseObjectNode(child_node);
            auto instance_out_classname = instance_out->GetClass()->Name();
            child_node->_deser_instance = instance_out;
            std::string uuids           = boost::uuids::to_string(instance_out->_uuid);
            child_node->_value.set<object_ptr_t>(instance_out);
            trackObject(instance_out->_uuid, instance_out);
            if (1)
              logchan_ds->log(
                  "instance<%p> class<%s> uuid<%s>", //
                  (void*) instance_out.get(),
                  instance_out_classname.c_str(),
                  uuids.c_str());
            break;
          }
          case rapidjson::kStringType: {
            auto strval = jsonobjnode.GetString();
            OrkAssert(strval == std::string("nil"));
            auto nil                    = object_ptr_t(nullptr);
            child_node->_deser_instance = nil;
            child_node->_value.set<object_ptr_t>(nil);
            break;
          }
          default:
            OrkAssert(false);
            break;
        }
      //////////////////////////////////////////////////////////////////////////
      } else if (subvalue.HasMember("object-ref")) { // object reference
      //////////////////////////////////////////////////////////////////////////
        const auto& jsonobjnode = subvalue["object-ref"];
        bool has_uuid           = jsonobjnode.HasMember("uuid-ref");
        OrkAssert(has_uuid);
        auto uuidstr = jsonobjnode["uuid-ref"].GetString();
        boost::uuids::string_generator gen;
        auto uuid                   = gen(uuidstr);
        auto instance_out           = findTrackedObject(uuid);
        child_node->_deser_instance = instance_out;
        child_node->_value.set<object_ptr_t>(instance_out);
      //////////////////////////////////////////////////////////////////////////
      } else { // "blind data" (pass-thru to custom deserializer)
      //////////////////////////////////////////////////////////////////////////
        for (auto it = subvalue.MemberBegin(); //
                  it != subvalue.MemberEnd(); //
                  ++it) {

          const auto& propkey  = it->name;
          const auto& propnode = it->value;
          auto propname = propkey.GetString();

          // create blind serdes::node_ptr tree
          auto propnode_serdes = pushNode(propname, NodeType::OBJECT);
          propnode_serdes->_parent = child_node;
          propnode_serdes->_impl.makeShared<JsonLeafNode>(propnode);
          auto propnode_child = _parseSubNode(propnode_serdes, propnode);
          propnode_serdes->_value = propnode_child->_value;
          popNode();

          child_node->_deser_blind_children.push_back(propnode_serdes);

          // OrkAssert(propnode.IsObject());

        }

      }
      break;
    }
    case rapidjson::kArrayType: {
      int numchildren = subvalue.Size();
      serdes::var_array_t vec;
      for( int ic=0; ic<numchildren; ic++ ) {
        const auto& childjsonvalue = subvalue[ic];
        switch( childjsonvalue.GetType() ) {
          case rapidjson::kObjectType: {
            OrkAssert(false);
            break;
          }
          case rapidjson::kNumberType: {
            svar64_t vv;
            vv.set<double>(childjsonvalue.GetDouble());
            vec.push_back( vv );
            break;
          }
          default:
            OrkAssert(false);
            break;
        }
        child_node->_value.set<serdes::var_array_t>(vec);
      }
      break;
    }
    case rapidjson::kNullType:
      child_node->_value.set<void*>(nullptr);
      break;
    case rapidjson::kFalseType:
      child_node->_value.set<bool>(false);
      break;
    case rapidjson::kTrueType:
      child_node->_value.set<bool>(true);
      break;
    case rapidjson::kStringType:
      child_node->_value.set<std::string>(subvalue.GetString());
      // logchan_ds->log("gotstr<%s>", child_node->_value.get<std::string>().c_str());
      break;
    case rapidjson::kNumberType:
      child_node->_value.set<double>(subvalue.GetDouble());
      break;
    default:
      OrkAssert(false);
  }
  popNode();
  return child_node;
}

//////////////////////////////////////////////////////////////////////////////

serdes::node_ptr_t JsonDeserializer::deserializeElement(node_ptr_t elemnode) {
  node_ptr_t childnode;
  /////////////////////////////////////////////////////
  // map element
  /////////////////////////////////////////////////////
  switch (elemnode->_type) {
    case NodeType::MAP_ELEMENT_LEAF:
    case NodeType::MAP_ELEMENT_OBJECT: {
      auto mapnode                    = elemnode->_parent;
      auto mapimplnode                = mapnode->_impl.getShared<JsonObjectNode>();
      const rapidjson::Value& objnode = mapimplnode->_jsonobjectnode;
      OrkAssert(mapimplnode->_iterator != objnode.MemberEnd());
       logchan_ds->log("mapnode key<%s>", mapimplnode->_iterator->name.GetString());
      const auto& childvalue = mapimplnode->_iterator->value;
      childnode              = _parseSubNode(elemnode, childvalue);
      childnode->_key        = mapimplnode->_iterator->name.GetString();
      mapimplnode->_iterator++;
    } break;
    case NodeType::ARRAY_ELEMENT_LEAF:
    case NodeType::ARRAY_ELEMENT_OBJECT: {
      auto arynode = elemnode->_parent;

      auto aryimplnode               = arynode->_impl.getShared<JsonArrayNode>();
      const rapidjson::Value& jarray = aryimplnode->_jsonarray;
      OrkAssert(aryimplnode->_iterator != jarray.End());
       logchan_ds->log("array element<%zu>", arynode->_index);
      const auto& childjsonvalue = *aryimplnode->_iterator;
      childnode                  = _parseSubNode(elemnode, childjsonvalue);
      aryimplnode->_iterator++;
    } break;
    default:
      OrkAssert(false);
      break;
  }
  return childnode;
  // OrkAssert(false);
}

//////////////////////////////////////////////////////////////////////////////

object_ptr_t JsonDeserializer::_parseObjectNode(serdes::node_ptr_t dsernode) {

  const rapidjson::Value& objnode = dsernode->_impl.getShared<JsonObjectNode>()->_jsonobjectnode;
  object_ptr_t instance_out       = nullptr;

  bool has_class = objnode.HasMember("class");
  bool has_uuid  = objnode.HasMember("uuid");
  OrkAssert(has_class);
  OrkAssert(has_uuid);

  auto classstr = objnode["class"].GetString();
  auto uuidstr  = objnode["uuid"].GetString();

  logchan_ds->log("_parseObjectNode classstr<%s>", classstr );

  boost::uuids::string_generator gen;
  auto uuid     = gen(uuidstr);
  auto clazz    = rtti::Class::FindClass(classstr);
  auto objclazz = dynamic_cast<object::ObjectClass*>(clazz);
  OrkAssert(objclazz);
  logchan_ds->log("_parseObjectNode objclazz<%p>", objclazz );
  const auto& description = objclazz->Description();

  //////////////////////////////////////////////
  // check for "reflect.no_instantiate"
  //  this should infer that the object is pre-instantiated
  //  and lifetime managed by parent
  //////////////////////////////////////////////

  auto top_prop = _property_stack.top();
  auto has_anno = top_prop //
                ? top_prop->annotation("reflect.no_instantiate").tryAs<bool>() //
                : false;

  if( has_anno ){
    auto parnode = dsernode->_parent;
    switch(parnode->_type){
      case NodeType::ARRAY_ELEMENT_LEAF:{
        auto arynode = parnode->_parent;
        auto aryobj = arynode->_deser_instance;
        auto aryclazz = aryobj->objectClass();
        int index = arynode->_index;
        auto top_prop_as_obj_array = dynamic_cast<const ITypedArray< object_ptr_t>*>(top_prop);
        top_prop_as_obj_array->get(instance_out, aryobj, index);
        if(0)printf( "aryobj<%p:%s> ARYINDEX<%d> top_prop_as_obj_array<%s> instance_out<%p>\n", //
                (void*) aryobj.get(), aryclazz->Name().c_str(), //
                index, top_prop_as_obj_array->_name.c_str(), //
                (void*) instance_out.get() );
        OrkAssert(instance_out!=nullptr);
        break;
      }
      case NodeType::ARRAY_ELEMENT_OBJECT:
        OrkAssert(false);
        break;
      default:
        OrkAssert(false);
        break;
    }
  }

  //////////////////////////////////////////////
  // if we have not been instantiated yet, do so
  //////////////////////////////////////////////

  if(instance_out==nullptr){
    instance_out        = objclazz->createShared();
  }

  //////////////////////////////////////////////


  instance_out->_uuid = uuid;

  logchan_ds->log("_parseObjectNode instance_out<%p>", instance_out.get() );

  instance_out->preDeserialize(*this);

  ///////////////////////////////////
  // deserialize properties
  ///////////////////////////////////

  const auto& propsnode = objnode["properties"];
  OrkAssert(propsnode.IsObject());

  ///////////////////////////////////

  for (auto it = propsnode.MemberBegin(); //
       it != propsnode.MemberEnd();
       ++it) {

    const auto& propkey  = it->name;
    const auto& propnode = it->value;

    // OrkAssert(propnode.IsObject());

    auto propname = propkey.GetString();
    auto prop     = description.property(propname);

    if (prop) {
       logchan_ds->log("found propname<%s> prop<%p>", propname, (void*) prop);
      dsernode->_property = prop;
      _property_stack.push(prop);
      auto child_node     = std::make_shared<Node>();

      child_node->_parent         = dsernode;
      child_node->_property       = prop;
      child_node->_deserializer   = this;
      child_node->_deser_instance = instance_out;
      child_node->_impl.makeShared<JsonLeafNode>(propnode);

      switch (propnode.GetType()) {
        case rapidjson::kObjectType: {
          child_node->_numchildren = propnode.MemberCount();
          auto jsonobjnode         = child_node->_impl.makeShared<JsonObjectNode>(propnode);
          jsonobjnode->_iterator   = propnode.MemberBegin();
          break;
        }
        case rapidjson::kArrayType: {
          child_node->_numchildren = propnode.Size();
          auto jsonarynode         = child_node->_impl.makeShared<JsonArrayNode>(propnode);
          jsonarynode->_iterator   = propnode.Begin();
          break;
        }
        case rapidjson::kNullType:
          child_node->_value.set<void*>(nullptr);
          break;
        case rapidjson::kFalseType:
          child_node->_value.set<bool>(false);
          break;
        case rapidjson::kTrueType:
          child_node->_value.set<bool>(true);
          break;
        case rapidjson::kStringType:
          child_node->_value.set<std::string>(propnode.GetString());
          break;
        case rapidjson::kNumberType:
          child_node->_value.set<double>(propnode.GetDouble());
          break;
        default:
          OrkAssert(false);
      }
      prop->deserialize(child_node);
      _property_stack.pop();
    } else { // drop property, no longer registered
      logchan_ds->log("dropping property<%s>", propname);
    }
  }
  instance_out->postDeserialize(*this, instance_out);

  ///////////////////////////////////
  return instance_out;
}

//////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
