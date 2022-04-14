////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORKTOOL_GED_DELEGATE_MAP_H_
#define _ORKTOOL_GED_DELEGATE_MAP_H_

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

class GedMapIoDriver : public IoDriverBase {
public:
  KeyDecoName mDecoKey;

  GedMapIoDriver(ObjModel& Model, const reflect::ObjectProperty* prop, Object* obj)
      : IoDriverBase(Model, prop, obj)
      , mDecoKey("", 0) {
  }
  void SetKey(const KeyDecoName& deco) {
    mDecoKey = deco;
  }

  void insert(const char* pname);
  void remove(const KeyDecoName& kdeco);
  void move(const KeyDecoName& kdeco, const char* pname);
  void duplicate(const KeyDecoName& kdeco, const char* pname);
  void importfile(const KeyDecoName& kdeco, const char* pname);
  void exportfile(const KeyDecoName& kdeco, const char* pname);

  /////////////////////////////////////////
  void SetValue(ork::Object* pobject);
  void GetValue(const ork::Object*& rp) const;
  /////////////////////////////////////////
  void SetValue(const ork::fvec3& pobject);
  void GetValue(ork::fvec3& rp) const;
  /////////////////////////////////////////
  void SetValue(PoolString ps);
  void GetValue(PoolString& ps) const;
  /////////////////////////////////////////
  void SetValue(file::Path pth);
  void GetValue(file::Path& pth) const;
  /////////////////////////////////////////
  void SetValue(float flt);
  void GetValue(float& flt) const;
  /////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class GedMapFactoryNode : public GedItemNode {
  GedMapIoDriver mIoDriver;
  any64 mKey;

  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;

  void DoDraw(lev2::Context* pTARG) final {
    GetSkin()->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);
    GetSkin()->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
    GetSkin()->DrawText(this, miX + 4, miY + 4, _propname.c_str());
  }

public:
  GedMapIoDriver& RefIODriver() {
    return mIoDriver;
  }

  GedMapFactoryNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
      : GedItemNode(mdl, name, prop, obj)
      , mIoDriver(mdl, prop, obj) {
  }

  void SetKey(const any64& v) {
    mKey = v;
  }
  const any64& GetKey() const {
    return mKey;
  }
};

///////////////////////////////////////////////////////////////////////////////

class MapTraverseSerializer : public reflect::serdes::LayerSerializer {
  ObjModel& mModel;
  const reflect::ObjectProperty* mProp;
  Object* mObject;
  PropTypeString mKeyString;
  KeyDecoName mKeyDeco;
  PropTypeString mValString;
  ConstString mMapValueTypeAnno;
  GedMapNode& mMapNode;
  int miMultiIndex;
  int miFloatCounter;
  PoolString mLastHint;
  fvec3 mVec3Acc;
  bool mbIsKey;

  bool IsMultiKey() const;
  PropTypeString GetDecoKeyString() const;

public:
  MapTraverseSerializer(
      GedMapNode& mapnode,
      ISerializer& serializer,
      ObjModel& model,
      ork::Object* pobj,
      const reflect::ObjectProperty* prop);

  bool IsKey() const {
    return mbIsKey;
  }

  bool Serialize(const bool&) override;
  bool Serialize(const char&) override;
  bool Serialize(const short&) override;
  bool Serialize(const int&) override;
  bool Serialize(const long&) override;
  bool Serialize(const float&) override;
  bool Serialize(const double&) override;
  bool serializeObject(rtti::castable_rawconstptr_t) override;
  bool Serialize(const PieceString&) override;

  void Hint(const PieceString& pstr) override;
  void Hint(const PieceString&, hintvar_t val) override;
};

///////////////////////////////////////////////////////////////////////////////

class MapItemWriteSerializer : public reflect::serdes::LayerDeserializer {
  enum EWRITETYPE {
    EWT_FLOAT = 0,
    EWT_CVECTOR3,
  };

  const reflect::IMap* mMapProp;
  reflect::serdes::NullDeserializer mNullDeser;
  GedMapIoDriver& mIoDriver;
  EWRITETYPE meWriteType;
  int miFloatIndex;
  bool mbIsKey;
  PoolString mLastHint;
  /////////////////////////////////////////////////////
  ork::ArrayString<1024> mArrayString;
  float mFloat;
  fvec3 mVector3;
  /////////////////////////////////////////////////////
  rtti::ICastable* mValueCastable;
  ork::PoolString mValuePoolString;
  /////////////////////////////////////////////////////
  /*virtual*/ void Hint(const PieceString& pstr);
  /*virtual*/ bool Deserialize(bool&);
  /*virtual*/ bool Deserialize(char&);
  /*virtual*/ bool Deserialize(short&);
  /*virtual*/ bool Deserialize(int&);
  /*virtual*/ bool Deserialize(long&);
  /*virtual*/ bool Deserialize(float&);
  /*virtual*/ bool Deserialize(double&);
  /*virtual*/ bool Deserialize(rtti::ICastable*&);
  /*virtual*/ bool Deserialize(MutableString&);
  /*virtual*/ bool Deserialize(ResizableString&);
  /*virtual*/ bool DeserializeData(unsigned char*, size_t);
  /////////////////////////////////////////////////////
  bool IsKey() const {
    return mbIsKey;
  }
  /////////////////////////////////////////////////////
public:
  /////////////////////////////////////////////////////
  MapItemWriteSerializer(GedMapIoDriver& iodriver);
  /////////////////////////////////////////////////////
  void Insert(const char* pchar);
  void Remove(const KeyDecoName& kdeco);
  void Move(const KeyDecoName& kdeco, const char* pchar);
  void Duplicate(const KeyDecoName& kdeco, const char* pchar);
  void Import(const KeyDecoName& kdeco, const char* pchar);
  void Export(const KeyDecoName& kdeco, const char* pchar);
  /////////////////////////////////////////////////////
  void SetValue(rtti::ICastable* castable);
  void SetValue(const char* pstr);
  void SetValue(float flt);
  void SetValue(const fvec3& flt);
  /////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class MapItemReadSerializer : public reflect::serdes::LayerSerializer {
  enum EREADTYPE {
    ERT_OBJECT = 0,
    ERT_NEWASSET,
    ERT_POOLSTRING,
    ERT_PATHOBJECT,
    ERT_FLOAT,
    ERT_CVECTOR3,
  };

  /////////////////////////////////////////////////////
  const reflect::IMap* mMapProp;
  reflect::serdes::NullSerializer mNullSer;
  const GedMapIoDriver& mIoDriver;
  /////////////////////////////////////////////////////
  const ork::Object* mpObject;
  ork::PoolString mPoolString;
  ork::file::Path mPathObject;
  float mFloat;
  fvec3 mVector3;
  EREADTYPE meReadType;
  int miFloatIndex;
  /////////////////////////////////////////////////////
  bool Serialize(const PieceString& value);
  bool Serialize(const rtti::ICastable* value);
  bool Serialize(const float& value);
  /////////////////////////////////////////////////////
public:
  /////////////////////////////////////////////////////
  MapItemReadSerializer(const GedMapIoDriver& ioDriver);
  /////////////////////////////////////////////////////
  const ork::asset::Asset* asset();
  PoolString GetPoolString();
  file::Path GetPathObject();
  float GetFloat();
  fvec3 Getfvec3();
  /////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class MapKeyWriter : public reflect::serdes::LayerDeserializer {
  reflect::serdes::NullDeserializer mNull;
  const GedMapIoDriver& mIoDriver;

  bool Deserialize(MutableString& mstr) {
    mstr.format("%s", mIoDriver.mDecoKey.mActualKey.c_str());
    return true;
  }
  bool Deserialize(float& rval) {
    rval = PropType<float>::FromString(mIoDriver.mDecoKey.mActualKey);
    return true;
  }
  bool Deserialize(int& rval) {
    rval = PropType<int>::FromString(mIoDriver.mDecoKey.mActualKey);
    return true;
  }

public:
  MapKeyWriter(const GedMapIoDriver& ioDriver)
      : reflect::serdes::LayerDeserializer(mNull)
      , mIoDriver(ioDriver)

  {
  }
};

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////

#endif
