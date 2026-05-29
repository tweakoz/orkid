////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <ork/file/path.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/quaternion.h>
#include <ork/math/cmatrix4.h>

// Geometry — orkid's standalone, attribute-based geometry container.
//
// A generic geometry detail: named CHANNELS scoped by an OWNER class
// (point / vertex / primitive / detail) plus polygon topology. It is
// deliberately decoupled from any one renderer mesh type — MicroMesh / XGM /
// submesh are CONSUMERS reached through adapters, not the container itself.
//
// Storage is AOS: each channel is an array of whole elements (fvec3, fvec4,
// ...), so a per-element query touches one packed element — the dominant
// geometry access pattern. A channel is a polymorphic GeomChannelBase (with a
// datatype tag) whose concrete GeomChannel<T> owns a std::vector<T> directly;
// reach the typed array via channelAs<T>(name).
//
// Owner element counts: point -> numPoints; vertex -> numVertices (one per
// polygon corner); primitive -> numPolys; detail -> 1 (global).
//
// Role: the interchange and intermediate-cache format for the dataflow
// geometry system. Serializes to a chunkfile (filetype "ogeo") so authored
// / cached meshes round-trip without being inlined into reflected JSON.
// Future: interchange converters (gltf / xgm / ...).

namespace ork::meshutil {

///////////////////////////////////////////////////////////////////////////////

enum class GeomChannelType : uint8_t {
  FLOAT = 0,
  INT,
  VEC2,
  VEC3,
  VEC4,
  QUAT,
  MTX4,
};

enum class GeomOwner : uint8_t {
  POINT = 0,
  VERTEX,
  PRIM,
  DETAIL,
};

static constexpr int kNumGeomOwners = 4;

///////////////////////////////////////////////////////////////////////////////
// GeomChannelBase — polymorphic base. Carries the runtime datatype tag so a
// channel's element type can be queried without a cast and to drive
// serialization dispatch / cloning.
///////////////////////////////////////////////////////////////////////////////

struct GeomChannelBase {
  GeomChannelBase(GeomChannelType dt)
      : _datatype(dt) {
  }
  virtual ~GeomChannelBase() = default;

  virtual size_t count() const                       = 0;
  virtual size_t elementBytes() const                = 0; // sizeof(T)
  virtual void resize(size_t n)                      = 0;
  virtual const void* rawData() const                = 0; // contiguous element storage
  virtual void* rawData()                            = 0;
  virtual std::shared_ptr<GeomChannelBase> clone() const = 0;

  GeomChannelType _datatype;
};

using geomchannel_ptr_t = std::shared_ptr<GeomChannelBase>;

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct GeomChannelTraits;
template <> struct GeomChannelTraits<float> { static constexpr GeomChannelType kType = GeomChannelType::FLOAT; };
template <> struct GeomChannelTraits<int>   { static constexpr GeomChannelType kType = GeomChannelType::INT;   };
template <> struct GeomChannelTraits<fvec2> { static constexpr GeomChannelType kType = GeomChannelType::VEC2;  };
template <> struct GeomChannelTraits<fvec3> { static constexpr GeomChannelType kType = GeomChannelType::VEC3;  };
template <> struct GeomChannelTraits<fvec4> { static constexpr GeomChannelType kType = GeomChannelType::VEC4;  };
template <> struct GeomChannelTraits<fquat> { static constexpr GeomChannelType kType = GeomChannelType::QUAT;  };
template <> struct GeomChannelTraits<fmtx4> { static constexpr GeomChannelType kType = GeomChannelType::MTX4;  };

///////////////////////////////////////////////////////////////////////////////
// GeomChannel<T> — concrete typed channel. Owns the AOS array directly.
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct GeomChannel : public GeomChannelBase {
  GeomChannel()
      : GeomChannelBase(GeomChannelTraits<T>::kType) {
  }
  size_t count() const final {
    return _data.size();
  }
  size_t elementBytes() const final {
    return sizeof(T);
  }
  void resize(size_t n) final {
    _data.resize(n);
  }
  const void* rawData() const final {
    return _data.data();
  }
  void* rawData() final {
    return _data.data();
  }
  std::shared_ptr<GeomChannelBase> clone() const final {
    auto c     = std::make_shared<GeomChannel<T>>();
    c->_data   = _data; // deep copy of the element array
    return c;
  }

  std::vector<T> _data;
};

template <typename T> using geomchannel_typed_ptr_t = std::shared_ptr<GeomChannel<T>>;

///////////////////////////////////////////////////////////////////////////////
// GeomAttributes — the ordered set of channels for ONE owner class.
///////////////////////////////////////////////////////////////////////////////

struct GeomAttributes {
  bool hasChannel(const std::string& name) const {
    return _channels.find(name) != _channels.end();
  }
  geomchannel_ptr_t channel(const std::string& name) const {
    auto it = _channels.find(name);
    return (it == _channels.end()) ? nullptr : it->second;
  }
  template <typename T> geomchannel_typed_ptr_t<T> channelAs(const std::string& name) const {
    auto it = _channels.find(name);
    if (it == _channels.end())
      return nullptr;
    return std::dynamic_pointer_cast<GeomChannel<T>>(it->second);
  }
  template <typename T> geomchannel_typed_ptr_t<T> createChannel(const std::string& name) {
    auto ch         = std::make_shared<GeomChannel<T>>();
    _channels[name] = ch;
    return ch;
  }
  void removeChannel(const std::string& name) {
    _channels.erase(name);
  }
  std::vector<std::string> channelNames() const;
  size_t numChannels() const {
    return _channels.size();
  }
  GeomAttributes cloned() const; // deep-copies every channel

  std::map<std::string, geomchannel_ptr_t> _channels;
};

///////////////////////////////////////////////////////////////////////////////

struct Geometry {

  Geometry()  = default;
  ~Geometry() = default;

  ///////////////////////////////////////////////
  // owner attribute sets
  ///////////////////////////////////////////////

  GeomAttributes&       attributes(GeomOwner owner);
  const GeomAttributes& attributes(GeomOwner owner) const;

  ///////////////////////////////////////////////
  // topology
  ///////////////////////////////////////////////

  // Point count — from the "P" point channel (fallback: largest point channel).
  int numPoints() const;
  // Vertex (polygon-corner) count.
  int numVertices() const {
    return int(_polyPointIndices.size());
  }
  int numPolys() const {
    return _polyVertexOffsets.empty() ? 0 : int(_polyVertexOffsets.size() - 1);
  }
  int numPrims() const {
    return numPolys();
  }
  // O(1) span of point indices for polygon `ipoly`.
  int polyVertexCount(int ipoly) const {
    return _polyVertexOffsets[ipoly + 1] - _polyVertexOffsets[ipoly];
  }
  const int* polyPointIndices(int ipoly) const {
    return _polyPointIndices.data() + _polyVertexOffsets[ipoly];
  }

  void addPoly(const std::vector<int>& point_indices);
  void addPolys(const std::vector<int>& flat_point_indices, int sides);

  // Newell face normal for polygon `ipoly` (reads the "P" point channel). O(valence).
  fvec3 computeFaceNormal(int ipoly) const;

  ///////////////////////////////////////////////

  std::shared_ptr<Geometry> clone() const;

  void writeChunkfile(const file::Path& path) const;
  static std::shared_ptr<Geometry> readChunkfile(const file::Path& path);

  ///////////////////////////////////////////////

  GeomAttributes _point;
  GeomAttributes _vertex;
  GeomAttributes _prim;
  GeomAttributes _detail;

  // Topology: prefix-offset array (size numPolys+1, leading 0) into a flat
  // corner->point index buffer. off[i]..off[i+1] is poly i's point span -> O(1).
  std::vector<int> _polyVertexOffsets;
  std::vector<int> _polyPointIndices;
};

using geometry_ptr_t = std::shared_ptr<Geometry>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::meshutil
