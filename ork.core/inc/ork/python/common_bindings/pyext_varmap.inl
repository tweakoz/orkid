#include <ork/python/pycodec.inl>
#include <ork/util/crc.h>
#include <ork/kernel/varmap.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
  struct VarMapKeyIterator2 {

    using map_t            = typename varmap::VarMap::map_t;
    using map_const_iter_t = map_t::const_iterator;

    VarMapKeyIterator2(varmap::varmap_ptr_t vmap)
        : _vmap(vmap) {
    }

    std::string operator*() const {
      return _it->first;
    }

    VarMapKeyIterator2 operator++() {
      ++_it;
      return *this;
    }

    bool operator==(const VarMapKeyIterator2& other) const {
      return _vmap == other._vmap;
    }

    static VarMapKeyIterator2 _begin(varmap::varmap_ptr_t vmap) {
      auto it = VarMapKeyIterator2(vmap);
      it._it  = vmap->_themap.begin();
      return it;
    }

    static VarMapKeyIterator2 _end(varmap::varmap_ptr_t vmap) {
      auto it = VarMapKeyIterator2(vmap);
      it._it  = vmap->_themap.end();
      return it;
    }

    varmap::varmap_ptr_t _vmap;
    map_const_iter_t _it;
  };

template <typename ADAPTER>
inline void _init_varmap(typename ADAPTER::module_t& module_core, typename ADAPTER::codec_ptr_t type_codec) {
  using namespace varmap;
  auto varmaptype_t =                                                         //
      py::class_<VarMap>(module_core, "VarMap") //
          .def(py::init<>())
          .def("update",[](py::dict dict) -> varmap_ptr_t {
            auto vmap = std::make_shared<VarMap>();
            for (auto item : dict) {
              auto key = py::cast<std::string>(item.first);
              auto val = item.second;
              vmap->setValueForKey(key, val);
            }
            return vmap;
          })
          .def(
              "__setattr__",                                                                    //
              [type_codec](varmap_ptr_t vmap, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                vmap->setValueForKey(key, varmap_val);
              })
          .def(
              "__getattr__",                                                                  //
              [type_codec](varmap_ptr_t vmap, const std::string& key) -> py::object { //
                auto varmap_val = vmap->valueForKey(key);
                auto python_val = type_codec->encode(varmap_val);
                return python_val;
              })
          .def("__len__", [](varmap_ptr_t vmap) -> size_t { return vmap->_themap.size(); })
          /*.def(
              "__iter__",
              [](varmap_ptr_t vmap) { //
                OrkAssert(false);
                return py::make_iterator( //
                  VarMapKeyIterator2::_begin(vmap), //
                  VarMapKeyIterator2::_end(vmap));
              },
              py::keep_alive<0, 1>())*/
          .def("__contains__", [](varmap_ptr_t vmap, std::string key) { //
            return vmap->_themap.contains(key);
          })
          .def("__getitem__", [type_codec](varmap_ptr_t vmap, std::string key) -> py::object { //
            auto it = vmap->_themap.find(key);
            if( it == vmap->_themap.end() )
              throw py::key_error("key not found");
            else {
              auto varmap_val = it->second;
              auto python_val = type_codec->encode(varmap_val);
              return python_val;
              }
          })
          .def("keys", [](varmap_ptr_t vmap) -> py::list {
            py::list rval;
            for( auto item : vmap->_themap ){
              rval.append(item.first);
            }
            return rval;           
          })
          .def("__repr__", [](varmap_ptr_t vmap) -> std::string {
            std::string rval;
            size_t numkeys = vmap->_themap.size();
            rval = FormatString("VarMap(nkeys:%zu)", numkeys );
            return rval;           
          })
          .def("dumpToString", [](varmap_ptr_t vmap) -> std::string {
            std::string rval = "varmap: {\n";
            for (const auto& item : vmap->_themap) {
              const auto& key = item.first;
              auto val = vmap->encodeAsString(key);
              //printf( "key<%s> val<%s>\n", key.c_str(), val.dumpToString().c_str() );
              rval += FormatString("  %s: %s\n", key.c_str(), val.c_str());
            }
            rval += "}\n";
            return rval;
          })
          .def("clone", [](varmap_ptr_t vmap) -> varmap_ptr_t {
            auto vmap_out = std::make_shared<VarMap>();
            (*vmap_out) = (*vmap);
            return vmap_out;           
          });
  //.def("__reversed__", [](varmap_ptr_t vmap) -> Sequence { return s.reversed(); })
  type_codec->template registerStdCodec<VarMap>(varmaptype_t);

} // _init_varmap
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::python {
