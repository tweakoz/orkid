////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <cstring>
#include <algorithm>

// The chunkfile codec serializes channel arrays as raw contiguous bytes, so
// the AOS element types must be tightly packed (no padding) for the byte
// layout to round-trip. Assert that here.
static_assert(sizeof(ork::fvec2) == 8,  "fvec2 must be 2 tightly-packed floats");
static_assert(sizeof(ork::fvec3) == 12, "fvec3 must be 3 tightly-packed floats");
static_assert(sizeof(ork::fvec4) == 16, "fvec4 must be 4 tightly-packed floats");
static_assert(sizeof(ork::fquat) == 16, "fquat must be 4 tightly-packed floats");
static_assert(sizeof(ork::fmtx4) == 64, "fmtx4 must be 16 tightly-packed floats");

namespace ork::meshutil {

static constexpr const char* kOrkGeoFileType = "ogeo";
static constexpr uint32_t    kOrkGeoVersion  = 1;

///////////////////////////////////////////////////////////////////////////////
// GeomAttributes
///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> GeomAttributes::channelNames() const {
  std::vector<std::string> out;
  out.reserve(_channels.size());
  for (const auto& kv : _channels)
    out.push_back(kv.first);
  return out;
}

GeomAttributes GeomAttributes::cloned() const {
  GeomAttributes out;
  for (const auto& kv : _channels)
    out._channels[kv.first] = kv.second->clone();
  return out;
}

///////////////////////////////////////////////////////////////////////////////
// Geometry
///////////////////////////////////////////////////////////////////////////////

GeomAttributes& Geometry::attributes(GeomOwner owner) {
  switch (owner) {
    case GeomOwner::POINT:  return _point;
    case GeomOwner::VERTEX: return _vertex;
    case GeomOwner::PRIM:   return _prim;
    case GeomOwner::DETAIL: return _detail;
  }
  return _point;
}
const GeomAttributes& Geometry::attributes(GeomOwner owner) const {
  return const_cast<Geometry*>(this)->attributes(owner);
}

///////////////////////////////////////////////////////////////////////////////

int Geometry::numPoints() const {
  auto it = _point._channels.find("P");
  if (it != _point._channels.end())
    return int(it->second->count());
  size_t mx = 0;
  for (const auto& kv : _point._channels)
    mx = std::max(mx, kv.second->count());
  return int(mx);
}

///////////////////////////////////////////////////////////////////////////////

void Geometry::addPoly(const std::vector<int>& point_indices) {
  if (_polyVertexOffsets.empty())
    _polyVertexOffsets.push_back(0);
  for (int p : point_indices)
    _polyPointIndices.push_back(p);
  _polyVertexOffsets.push_back(int(_polyPointIndices.size()));
}

void Geometry::addPolys(const std::vector<int>& flat_point_indices, int sides) {
  OrkAssert(sides > 0);
  OrkAssert((flat_point_indices.size() % size_t(sides)) == 0);
  if (_polyVertexOffsets.empty())
    _polyVertexOffsets.push_back(0);
  size_t start = _polyPointIndices.size(); // == _polyVertexOffsets.back()
  _polyPointIndices.insert(_polyPointIndices.end(), flat_point_indices.begin(), flat_point_indices.end());
  size_t npoly = flat_point_indices.size() / size_t(sides);
  for (size_t i = 1; i <= npoly; i++)
    _polyVertexOffsets.push_back(int(start + i * size_t(sides)));
}

///////////////////////////////////////////////////////////////////////////////

fvec3 Geometry::computeFaceNormal(int ipoly) const {
  fvec3 n(0, 0, 0);
  auto P = _point.channelAs<fvec3>("P");
  if (not P)
    return n;
  const auto& pts = P->_data;
  int cnt         = polyVertexCount(ipoly);
  const int* idx  = polyPointIndices(ipoly);
  // Newell's method — robust for non-planar / arbitrary polygons.
  for (int i = 0; i < cnt; i++) {
    const fvec3& cur = pts[idx[i]];
    const fvec3& nxt = pts[idx[(i + 1) % cnt]];
    n.x += (cur.y - nxt.y) * (cur.z + nxt.z);
    n.y += (cur.z - nxt.z) * (cur.x + nxt.x);
    n.z += (cur.x - nxt.x) * (cur.y + nxt.y);
  }
  return n.normalized();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<Geometry> Geometry::clone() const {
  auto g               = std::make_shared<Geometry>();
  g->_point            = _point.cloned();
  g->_vertex           = _vertex.cloned();
  g->_prim             = _prim.cloned();
  g->_detail           = _detail.cloned();
  g->_polyVertexOffsets = _polyVertexOffsets;
  g->_polyPointIndices  = _polyPointIndices;
  return g;
}

///////////////////////////////////////////////////////////////////////////////
// chunkfile IO
//
// Layout: stream "HEADER" carries scalars / names / counts; stream "DATA"
// carries the raw channel + topology byte payloads in the same order they are
// announced in HEADER. swapbytes_dynamic is a no-op at host endian, so the
// scalar AddItem / raw Write idiom round-trips natively (same as XGM).
///////////////////////////////////////////////////////////////////////////////

namespace {
void writeAttributes(const GeomAttributes& attrs, chunkfile::OutputStream* hdr, chunkfile::OutputStream* dat, chunkfile::Writer& w) {
  hdr->AddItem<uint32_t>(uint32_t(attrs._channels.size()));
  for (const auto& kv : attrs._channels) {
    auto ch = kv.second;
    hdr->AddIndexedString(kv.first, w);
    hdr->AddItem<uint8_t>(uint8_t(ch->_datatype));
    hdr->AddItem<uint32_t>(uint32_t(ch->count()));
    size_t nbytes = ch->count() * ch->elementBytes();
    if (nbytes)
      dat->Write((const uint8_t*)ch->rawData(), nbytes);
  }
}

template <typename T>
void readTypedChannel(GeomAttributes& attrs, const std::string& name, uint32_t count, chunkfile::InputStream* dat) {
  auto ch = attrs.createChannel<T>(name);
  ch->_data.resize(count);
  size_t nbytes = size_t(count) * sizeof(T);
  if (nbytes) {
    auto bytes = dat->readData(nbytes);
    std::memcpy(ch->_data.data(), bytes.data(), nbytes);
  }
}

void readAttributes(GeomAttributes& attrs, chunkfile::InputStream* hdr, chunkfile::InputStream* dat, const chunkfile::Reader& r) {
  uint32_t nchan = hdr->ReadItem<uint32_t>();
  for (uint32_t i = 0; i < nchan; i++) {
    std::string name   = hdr->ReadIndexedString(r);
    GeomChannelType dt = GeomChannelType(hdr->ReadItem<uint8_t>());
    uint32_t count     = hdr->ReadItem<uint32_t>();
    switch (dt) {
      case GeomChannelType::FLOAT: readTypedChannel<float>(attrs, name, count, dat); break;
      case GeomChannelType::INT:   readTypedChannel<int>(attrs,   name, count, dat); break;
      case GeomChannelType::VEC2:  readTypedChannel<fvec2>(attrs, name, count, dat); break;
      case GeomChannelType::VEC3:  readTypedChannel<fvec3>(attrs, name, count, dat); break;
      case GeomChannelType::VEC4:  readTypedChannel<fvec4>(attrs, name, count, dat); break;
      case GeomChannelType::QUAT:  readTypedChannel<fquat>(attrs, name, count, dat); break;
      case GeomChannelType::MTX4:  readTypedChannel<fmtx4>(attrs, name, count, dat); break;
      default: OrkAssert(false);
    }
  }
}
} // namespace

///////////////////////////////////////////////////////////////////////////////

void Geometry::writeChunkfile(const file::Path& path) const {
  chunkfile::Writer w(kOrkGeoFileType);
  auto hdr = w.AddStream("HEADER");
  auto dat = w.AddStream("DATA");

  hdr->AddItem<uint32_t>(kOrkGeoVersion);
  hdr->AddItem<uint32_t>(uint32_t(numPoints()));
  hdr->AddItem<uint32_t>(uint32_t(numPolys()));

  // attribute sets, in owner order POINT, VERTEX, PRIM, DETAIL
  writeAttributes(_point,  hdr, dat, w);
  writeAttributes(_vertex, hdr, dat, w);
  writeAttributes(_prim,   hdr, dat, w);
  writeAttributes(_detail, hdr, dat, w);

  // topology
  uint32_t npoly = uint32_t(numPolys());
  hdr->AddItem<uint32_t>(npoly);
  for (uint32_t i = 0; i < npoly; i++)
    hdr->AddItem<uint32_t>(uint32_t(polyVertexCount(int(i))));
  hdr->AddItem<uint32_t>(uint32_t(_polyPointIndices.size()));
  if (not _polyPointIndices.empty())
    dat->Write((const uint8_t*)_polyPointIndices.data(), _polyPointIndices.size() * sizeof(int));

  w.WriteToFile(path);
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<Geometry> Geometry::readChunkfile(const file::Path& path) {
  chunkfile::DefaultLoadAllocator alloc;
  chunkfile::Reader r(path, kOrkGeoFileType, alloc);
  if (not r.IsOk())
    return nullptr;

  auto geo = std::make_shared<Geometry>();
  auto hdr = r.GetStream("HEADER");
  auto dat = r.GetStream("DATA");
  OrkAssert(hdr != nullptr and dat != nullptr);

  uint32_t version = hdr->ReadItem<uint32_t>();
  OrkAssert(version == kOrkGeoVersion);
  hdr->ReadItem<uint32_t>(); // numPoints (advisory; recomputed from channels)
  hdr->ReadItem<uint32_t>(); // numPolys  (advisory; rebuilt from offsets below)

  readAttributes(geo->_point,  hdr, dat, r);
  readAttributes(geo->_vertex, hdr, dat, r);
  readAttributes(geo->_prim,   hdr, dat, r);
  readAttributes(geo->_detail, hdr, dat, r);

  // topology
  uint32_t npoly = hdr->ReadItem<uint32_t>();
  std::vector<int> counts(npoly);
  for (uint32_t i = 0; i < npoly; i++)
    counts[i] = int(hdr->ReadItem<uint32_t>());
  uint32_t nidx = hdr->ReadItem<uint32_t>();
  geo->_polyPointIndices.resize(nidx);
  if (nidx) {
    auto bytes = dat->readData(size_t(nidx) * sizeof(int));
    std::memcpy(geo->_polyPointIndices.data(), bytes.data(), size_t(nidx) * sizeof(int));
  }
  geo->_polyVertexOffsets.clear();
  geo->_polyVertexOffsets.push_back(0);
  for (uint32_t i = 0; i < npoly; i++)
    geo->_polyVertexOffsets.push_back(geo->_polyVertexOffsets.back() + counts[i]);

  return geo;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::meshutil
