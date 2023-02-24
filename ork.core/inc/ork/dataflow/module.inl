////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

////////////////////////////////////////////
template <typename plug_traits, typename... A> //
std::shared_ptr<inplugdata<plug_traits>> ModuleData::createInputPlug(moduledata_ptr_t m, A&&... args) {
  using plug_impl_type = inplugdata<plug_traits>;
  auto plg             = std::make_shared<plug_impl_type>(m, std::forward<A>(args)...);
  m->addInput(plg);
  return plg;
}
////////////////////////////////////////////
template <typename plug_traits, typename... A> //
std::shared_ptr<outplugdata<plug_traits>> ModuleData::createOutputPlug(moduledata_ptr_t m, A&&... args) {
  using plug_impl_type = outplugdata<plug_traits>;
  auto plg             = std::make_shared<plug_impl_type>(m, std::forward<A>(args)...);
  m->addOutput(plg);
  return plg;
}
////////////////////////////////////////////
template <typename plug_traits> //
std::shared_ptr<inplugdata<plug_traits>> ModuleData::typedInput(int idx) {
  inplugdata_ptr_t plug = input(idx);
  return std::dynamic_pointer_cast<inplugdata<plug_traits>>(plug);
}
////////////////////////////////////////////
template <typename plug_traits> //
std::shared_ptr<outplugdata<plug_traits>> ModuleData::typedOutput(int idx) {
  outplugdata_ptr_t plug = output(idx);
  return std::dynamic_pointer_cast<outplugdata<plug_traits>>(plug);
}
////////////////////////////////////////////
template <typename plug_traits> //
std::shared_ptr<inplugdata<plug_traits>> ModuleData::typedInputNamed(const std::string& named) const {
  inplugdata_ptr_t plug = inputNamed(named);
  return std::dynamic_pointer_cast<inplugdata<plug_traits>>(plug);
}
////////////////////////////////////////////
template <typename plug_traits> //
std::shared_ptr<outplugdata<plug_traits>> ModuleData::typedOutputNamed(const std::string& named) const {
  outplugdata_ptr_t plug = outputNamed(named);
  return std::dynamic_pointer_cast<outplugdata<plug_traits>>(plug);
}

template <typename T> std::shared_ptr<T> typedModuleData(object_ptr_t m) {
  return std::dynamic_pointer_cast<T>(m);
}

////////////////////////////////////////////
template <typename plug_type> //
std::shared_ptr<inpluginst<plug_type>> DgModuleInst::typedInput(int idx) const {
  inpluginst_ptr_t plug = input(idx);
  return std::dynamic_pointer_cast<inpluginst<plug_type>>(plug);
}
////////////////////////////////////////////
template <typename plug_type> //
std::shared_ptr<outpluginst<plug_type>> DgModuleInst::typedOutput(int idx) const {
  outpluginst_ptr_t plug = output(idx);
  return std::dynamic_pointer_cast<outpluginst<plug_type>>(plug);
}
////////////////////////////////////////////
template <typename plug_type> //
std::shared_ptr<inpluginst<plug_type>> DgModuleInst::typedInputNamed(const std::string& named) const {
  inpluginst_ptr_t plug = inputNamed(named);
  return std::dynamic_pointer_cast<inpluginst<plug_type>>(plug);
}
////////////////////////////////////////////
template <typename plug_type> //
std::shared_ptr<outpluginst<plug_type>> DgModuleInst::typedOutputNamed(const std::string& named) const {
  outpluginst_ptr_t plug = outputNamed(named);
  return std::dynamic_pointer_cast<outpluginst<plug_type>>(plug);
}

template <typename T> std::shared_ptr<T> typedModuleInst(moduleinst_ptr_t m) {
  return std::dynamic_pointer_cast<T>(m);
}
} // namespace ork::dataflow
