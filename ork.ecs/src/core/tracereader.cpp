////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/system.h>
#include <ork/ecs/datatable.h>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>

#include "message_private.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <rapidjson/reader.h>
#include <rapidjson/document.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

using namespace ::ork;

///////////////////////////////////////////////////////////////////////////////

struct ReaderItem {
  svar512_t _data;
  float _timestamp = 0.0f;
};

struct ReaderImpl {

  using allocator_t = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>*;
  rapidjson::Document _document;
  Controller* _controller = nullptr;
  allocator_t _allocator;
  std::vector<ReaderItem> _deserialized;

  ReaderImpl(Controller* c, std::string jsondata)
      : _document()
      , _controller(c) {
    _allocator = &_document.GetAllocator();
    parse(jsondata);
  }
  ///////////////////////////////////////////////
  void enqueueItem(float timestamp, const svar512_t& data) {
    ReaderItem ritem;
    ritem._data      = data;
    ritem._timestamp = timestamp;
    _deserialized.push_back(ritem);
  }
  ///////////////////////////////////////////////
  svar64_t parseKvItem(const rapidjson::Value& jsonval) {

    svar64_t rval;
    if (jsonval.IsObject()) {
      if (jsonval.HasMember("token")) {
        token_t token;
        if (jsonval["token"].IsString()) {
          token = tokenize(jsonval["token"].GetString());
        } else if (jsonval["token"].IsNumber()) {
          double as_dbl = jsonval["token"].GetDouble();
          token._hashed = uint64_t(as_dbl);
          printf("as_dbl<%g> token._hashed<%llx>\n", as_dbl, token._hashed);
        }
        rval = token;
      } else if (jsonval.HasMember("int")) {
        rval.set<int>(jsonval["int"].GetInt());
      } else if (jsonval.HasMember("uint64_t")) {
        double as_dbl = jsonval["token"].GetDouble();
        rval.set<uint64_t>(as_dbl);
      } else if (jsonval.HasMember("float")) {
        rval.set<float>(jsonval["float"].GetDouble());
      } else if (jsonval.HasMember("double")) {
        rval.set<double>(jsonval["double"].GetDouble());
      } else if (jsonval.HasMember("fvec3")) {
        auto ary = jsonval["fvec3"].GetArray();
        rval.make<fvec3>(
            ary[0].GetDouble(), //
            ary[1].GetDouble(), //
            ary[2].GetDouble());
      } else if (jsonval.HasMember("fquat")) {
        auto ary = jsonval["fquat"].GetArray();
        rval.make<fquat>(
            ary[0].GetDouble(), //
            ary[1].GetDouble(), //
            ary[2].GetDouble(), //
            ary[3].GetDouble());
      } else if (jsonval.HasMember("table")) {
        auto& out_table       = rval.make<DataTable>();
        const auto& SRC_TABLE = jsonval["table"].GetArray();
        out_table._items.resize(SRC_TABLE.Size());
        for (size_t i = 0; i < SRC_TABLE.Size(); i++) {
          const auto& SRC_ITEM   = SRC_TABLE[i];
          auto& DST_ITEM         = out_table._items[i];
          DST_ITEM._key._encoded = parseKvItem(SRC_ITEM["key"]);
          DST_ITEM._val._encoded = parseKvItem(SRC_ITEM["val"]);
        }

      } else if (jsonval.HasMember("stdstr")) {
        rval.set<std::string>(jsonval["stdstr"].GetString());
      } else if (jsonval.HasMember("modeldata")) {

        // TODO - add to scene's data declaration before simulation starts!

        auto path = jsonval["modeldata"]["path"].GetString();
        printf("Path<%s>\n", path);
        auto& out_modeldata = rval.makeShared<lev2::ModelDrawableData>(path);
        _controller->forceRetain(rval);
      } else if (jsonval.HasMember("respref")) {
        ResponseRef rr;
        rr._responseID = jsonval["respref"]["respID"].GetInt();
        rval           = rr;
      } else {
        OrkAssert(false);
      }
    } else {
      OrkAssert(false);
    }
    return rval;
  }
  ///////////////////////////////////////////////
  void parse(std::string jsondata) {
    _document.Parse(jsondata.c_str());

    bool is_array = _document.IsArray();
    OrkAssert(is_array);

    for (size_t i = 0; i < _document.Size(); i++) {
      const auto& ITEM = _document[i];
      float timestamp  = ITEM["timestamp"].GetDouble();
      printf("parsing item<%d> timestamp<%g>\n", int(i), timestamp);
      /////////////////////////////////////////////////////////
      if (ITEM.HasMember("FindSystem")) {
        const auto& EVENTTYPE = ITEM["FindSystem"];
        uint64_t sysID        = EVENTTYPE["sysID"].GetInt();
        svar512_t out_item;
        auto& out_FSYS          = out_item.make<impl::_FindSystem>();
        out_FSYS._sysref._sysID = sysID;

        printf("sysID<%llu>\n", out_FSYS._sysref._sysID);

        // OrkAssert(false);
        out_FSYS._syskey = EVENTTYPE["syskey"].GetString();
        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("SystemRequest")) {
        const auto& EVENTTYPE = ITEM["SystemRequest"];

        svar512_t out_item;
        auto& out_SRQ = out_item.make<impl::_SystemRequest>();

        out_SRQ._sysref._sysID       = (int)EVENTTYPE["sysID"].GetInt();
        out_SRQ._requestID           = parseKvItem(EVENTTYPE["reqID"]).get<token_t>();
        out_SRQ._eventData           = parseKvItem(EVENTTYPE["reqDATA"]);
        out_SRQ._respref._responseID = EVENTTYPE["responseID"].GetInt();
        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("SystemEvent")) {
        const auto& EVENTTYPE = ITEM["SystemEvent"];

        svar512_t out_item;
        auto& out_SEV = out_item.make<impl::_SystemEvent>();

        out_SEV._sysref._sysID = (int)EVENTTYPE["sysID"].GetInt();
        out_SEV._eventID       = parseKvItem(EVENTTYPE["evnID"]).get<token_t>();
        out_SEV._eventData     = parseKvItem(EVENTTYPE["evnDATA"]);

        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("FindComponent")) {
        const auto& EVENTTYPE = ITEM["FindComponent"];
        uint64_t compID       = EVENTTYPE["compID"].GetInt();
        svar512_t out_item;
        auto& out_FCMP            = out_item.make<impl::_FindComponent>();
        out_FCMP._entref._entID   = (int)EVENTTYPE["entID"].GetInt();
        out_FCMP._compref._compID = (int)EVENTTYPE["compID"].GetInt();
        auto classname            = EVENTTYPE["class"].GetString();
        auto clazz                = rtti::Class::FindClass(classname);
        out_FCMP._compclazz       = clazz;
        OrkAssert(clazz != nullptr);
        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("ComponentEvent")) {
        const auto& EVENTTYPE = ITEM["ComponentEvent"];
        svar512_t out_item;
        auto& out_CEV = out_item.make<impl::_ComponentEvent>();

        out_CEV._compref._compID = (int)EVENTTYPE["compID"].GetInt();
        out_CEV._eventID         = parseKvItem(EVENTTYPE["evnID"]).get<token_t>();
        out_CEV._eventData       = parseKvItem(EVENTTYPE["evnDATA"]);

        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("ComponentRequest")) {
        const auto& EVENTTYPE = ITEM["ComponentRequest"];
        svar512_t out_item;
        auto& out_CRQ = out_item.make<impl::_ComponentRequest>();

        out_CRQ._compref._compID     = (int)EVENTTYPE["compID"].GetInt();
        out_CRQ._requestID           = parseKvItem(EVENTTYPE["reqID"]).get<token_t>();
        out_CRQ._eventData           = parseKvItem(EVENTTYPE["reqDATA"]);
        out_CRQ._respref._responseID = EVENTTYPE["responseID"].GetInt();

        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("SpawnAnonDynamic")) {
        const auto& EVENTTYPE = ITEM["SpawnAnonDynamic"];
        svar512_t out_item;
        auto& out_SAD = out_item.make<impl::_SpawnAnonDynamic>();

        out_SAD._entref._entID = EVENTTYPE["entID"].GetInt();

        const auto& SRC_SREC   = EVENTTYPE["SREC"];
        const auto& SRC_XF     = SRC_SREC["xf"];
        const auto& SRC_XF_POS = SRC_XF["pos"];
        const auto& SRC_XF_ROT = SRC_XF["rot"];
        const auto& SRC_XF_SCA = SRC_XF["scale"];

        auto pos = fvec3(
            SRC_XF_POS[0].GetDouble(), //
            SRC_XF_POS[1].GetDouble(), //
            SRC_XF_POS[2].GetDouble());
        auto rot = fquat(
            SRC_XF_ROT[0].GetDouble(), //
            SRC_XF_ROT[1].GetDouble(), //
            SRC_XF_ROT[2].GetDouble(), //
            SRC_XF_ROT[3].GetDouble());

        float sca = SRC_XF_SCA.GetDouble();

        auto spawnrec                        = std::make_shared<SpawnData>();
        spawnrec->transform()->_translation  = pos;
        spawnrec->transform()->_rotation     = rot;
        spawnrec->transform()->_uniformScale = sca;

        out_SAD._spawn_rec = spawnrec;

        const auto& edataname   = EVENTTYPE["edata"];
        out_SAD._SAD->_edataname = AddPooledString(edataname.GetString());

        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else if (ITEM.HasMember("Despawn")) {
        const auto& EVENTTYPE = ITEM["Despawn"];
        svar512_t out_item;
        auto& out_DSP          = out_item.make<impl::_Despawn>();
        out_DSP._entref._entID = (int)EVENTTYPE["entID"].GetInt();
        enqueueItem(timestamp, out_item);
      }
      /////////////////////////////////////////////////////////
      else {
        OrkAssert(false);
      }
    }
  }
};

///////////////////////////////////////////////////////////////////////////////

Controller::TraceReader::TraceReader(Controller* c, file::Path path)
    : _controller(c) {

  ///////////////////////////////////
  File trace_file(path, EFM_READ);
  OrkAssert(trace_file.IsOpen());
  size_t file_length    = 0;
  EFileErrCode eFileErr = trace_file.GetLength(file_length);
  std::string json_data;
  json_data.resize(file_length + 1);
  eFileErr               = trace_file.Read(json_data.data(), file_length);
  json_data[file_length] = 0;

  auto impl = _impl.makeShared<ReaderImpl>(_controller, json_data);

  for (auto item : impl->_deserialized) {

    const auto& data = item._data;
    float timestamp  = item._timestamp;

    /////////////////////////////////////////////////////////////////
    if (auto as_typed = data.tryAs<impl::_SpawnNamedDynamic>()) {
      const auto& value = as_typed.value();
      OrkAssert(false);
    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_SpawnAnonDynamic>()) {
      const auto& value = as_typed.value();

      auto op = [=]() {
        auto req = std::make_shared<Request>();
        req->_requestID = RequestID::SPAWN_DYNAMIC_ANON;
        uint64_t objID = _controller->_objectIdCounter.fetch_add(1);
        OrkAssert(objID == value._entref._entID);
        auto& IMPL      = req->_payload.make<impl::_SpawnAnonDynamic>();
        IMPL._SAD       = value._SAD;
        IMPL._spawn_rec = _controller->_scenedata->findTypedObject<SpawnData>(value._SAD->_edataname);
        OrkAssert(IMPL._spawn_rec);
        ent_ref_t eref;
        IMPL._entref._entID = objID;
        _controller->_enqueueRequest(req);
      };

      _controller->presimDelayedOperation(timestamp, op);

    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_Despawn>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        auto simevent = std::make_shared<Event>();
        simevent->_eventID = EventID::DESPAWN;
        auto& DEV         = simevent->_payload.make<impl::_Despawn>();
        DEV._entref       = value._entref;
        _controller->_enqueueEvent(simevent);
      };
      _controller->presimDelayedOperation(timestamp, op);
    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_FindSystem>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        uint64_t ID = _controller->_objectIdCounter.fetch_add(1);

        printf("FINDSYS ID<%llx> TRID<%llx>\n", ID, value._sysref._sysID);

        OrkAssert(ID == value._sysref._sysID);

        //////////////////////////////////////////////////////
        // notify sim to update reference
        //////////////////////////////////////////////////////

        auto simevent = std::make_shared<Event>();
        simevent->_eventID = EventID::FIND_SYSTEM;
        auto& FSYS        = simevent->_payload.make<impl::_FindSystem>();

        FSYS._sysref = SystemRef{._sysID = ID};
        FSYS._syskey = value._syskey;

        _controller->_enqueueEvent(simevent);
      };
      _controller->presimDelayedOperation(timestamp, op);
    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_FindComponent>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        uint64_t ID = _controller->_objectIdCounter.fetch_add(1);

        printf("FINDCOMP ID<%llu> TRID<%llu>\n", ID, value._compref._compID);

        OrkAssert(ID == value._compref._compID);

        //////////////////////////////////////////////////////
        // notify sim to update reference
        //////////////////////////////////////////////////////

        auto simevent = std::make_shared<Event>();
        simevent->_eventID = EventID::FIND_COMPONENT;
        auto& FCOMP       = simevent->_payload.make<impl::_FindComponent>();

        FCOMP._entref    = value._entref;
        FCOMP._compclazz = value._compclazz; // T::GetClassStatic();
        FCOMP._compref   = value._compref;   // ComponentRef({._compID=ID});

        _controller->_enqueueEvent(simevent);
      };
      _controller->presimDelayedOperation(timestamp, op);
    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_SystemEvent>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        auto simevent = std::make_shared<Event>();
        simevent->_eventID = EventID::SYSTEM_EVENT;
        auto& SEV         = simevent->_payload.make<impl::_SystemEvent>();

        SEV._sysref    = value._sysref;
        SEV._eventID   = value._eventID;
        SEV._eventData = value._eventData;

        _controller->_enqueueEvent(simevent);
      };
      _controller->presimDelayedOperation(timestamp, op);
    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_SystemRequest>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        auto simrequest = std::make_shared<Request>();
        simrequest->_requestID = RequestID::SYSTEM_REQUEST;

        uint64_t objID = _controller->_objectIdCounter.fetch_add(1);

        printf("SYSREQ ID<%llu> TRID<%llu>\n", objID, value._respref._responseID);

        OrkAssert(objID == value._respref._responseID);

        auto rref = ResponseRef{._responseID = objID};

        auto& SRQ = simrequest->_payload.make<impl::_SystemRequest>();

        SRQ._sysref    = value._sysref;
        SRQ._requestID = value._requestID;
        SRQ._eventData = value._eventData;
        SRQ._respref   = value._respref;

        _controller->_enqueueRequest(simrequest);
      };
      _controller->presimDelayedOperation(timestamp, op);

    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_ComponentEvent>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        auto simevent = std::make_shared<Event>();
        simevent->_eventID = EventID::COMPONENT_EVENT;
        auto& CEV         = simevent->_payload.make<impl::_ComponentEvent>();

        CEV._compref   = value._compref;
        CEV._eventID   = value._eventID;
        CEV._eventData = value._eventData;

        _controller->_enqueueEvent(simevent);
      };
      _controller->presimDelayedOperation(timestamp, op);
    }
    /////////////////////////////////////////////////////////////////
    else if (auto as_typed = data.tryAs<impl::_ComponentRequest>()) {
      const auto& value = as_typed.value();
      auto op           = [=]() {
        auto simrequest = std::make_shared<Request>();
        simrequest->_requestID = RequestID::COMPONENT_REQUEST;
        auto& CRQ             = simrequest->_payload.make<impl::_ComponentRequest>();

        uint64_t objID = _controller->_objectIdCounter.fetch_add(1);
        OrkAssert(objID == value._respref._responseID);
        printf("COMPREQ ID<%llu> TRID<%llu>\n", objID, value._respref._responseID);

        auto rref = ResponseRef{._responseID = objID};

        CRQ._compref   = value._compref;
        CRQ._requestID = value._requestID;
        CRQ._eventData = value._eventData;
        CRQ._respref   = value._respref;

        _controller->_enqueueRequest(simrequest);
      };
      _controller->presimDelayedOperation(timestamp, op);

    }
    /////////////////////////////////////////////////////////////////
    else {
      OrkAssert(false);
    }
  }
  // OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

Controller::TraceReader::~TraceReader() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
