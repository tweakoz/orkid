#pragma once

#include <ork/orktypes.h>
#include <ork/orkstd.h>
#include <ork/kernel/varmap.inl>
#include <ork/util/scanner.h>
#include <unordered_set>
#include <unordered_map>
#include <ork/util/crc.h>
#include <ork/util/parser.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////

template <typename impl_t> std::shared_ptr<impl_t> MatchAttempt::asShared() {
  return _impl.getShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> MatchAttempt::makeShared() {
  return _impl.makeShared<impl_t>();
}
template <typename impl_t> attempt_cast<std::shared_ptr<impl_t>> MatchAttempt::tryAsShared() {
  return _impl.tryAsShared<impl_t>();
}
template <typename impl_t> attempt_cast<impl_t> MatchAttempt::tryAs() {
  return _impl.tryAs<impl_t>();
}
template <typename impl_t> attempt_cast_const<impl_t> MatchAttempt::tryAs() const {
  return _impl.tryAs<impl_t>();
}
template <typename impl_t> bool MatchAttempt::isShared() const {
  return _impl.isShared<impl_t>();
}

//////////////////////////////////////////////////////////////

template <typename impl_t> std::shared_ptr<impl_t> Match::asShared() {
  return _impl.getShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> Match::makeShared() {
  return _impl.makeShared<impl_t>();
}
template <typename impl_t> attempt_cast_const<std::shared_ptr<impl_t>> Match::tryAsShared() const {
  return _impl.tryAsShared<impl_t>();
}
template <typename impl_t> attempt_cast<impl_t> Match::tryAs() {
  return _impl.tryAs<impl_t>();
}
template <typename impl_t> attempt_cast_const<impl_t> Match::tryAs() const {
  return _impl.tryAs<impl_t>();
}
template <typename impl_t> bool Match::isShared() const {
  return _impl.isShared<impl_t>();
}
template <typename user_t> std::shared_ptr<user_t> Match::sharedForKey(std::string named) {
  using ptr_t = std::shared_ptr<user_t>;
  return _uservars.typedValueForKey<ptr_t>(named).value();
}
template <typename user_t> std::shared_ptr<user_t> Match::makeSharedForKey(std::string named) {
  return _uservars.makeSharedForKey<user_t>(named);
}
template <typename user_t> void Match::setSharedForKey(std::string named, std::shared_ptr<user_t> ptr) {
  return _uservars.set<std::shared_ptr<user_t>>(named,ptr);
}

template <typename impl_t> std::shared_ptr<impl_t> Match::followImplAsShared() {
  if (auto as_proxy = _impl.tryAsShared<Proxy>()) {
    return as_proxy.value()->followAsShared<impl_t>();
  }
  return _impl.tryAsShared<impl_t>().value();
}

//////////////////////////////////////////////////////////////

template <typename impl_t> std::shared_ptr<impl_t> SequenceAttempt::itemAsShared(int index) {
  return _items[index]->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> SequenceAttempt::tryItemAsShared(int index) {
  if(index>=_items.size())
    return nullptr;
  return _items[index]->tryAsShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> GroupAttempt::itemAsShared(int index) {
  return _items[index]->asShared<impl_t>();
}

template <typename impl_t> std::shared_ptr<impl_t> NOrMoreAttempt::itemAsShared(int index) {
  return _items[index]->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> OptionalAttempt::asShared() {
  return _subitem->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> OneOfAttempt::asShared() {
  return _selected->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> ProxyAttempt::asShared() {
  return _selected->asShared<impl_t>();
}

//////////////////////////////////////////////////////////////

template <typename impl_t> std::shared_ptr<impl_t> Sequence::itemAsShared(int index) {
  return _items[index]->asShared<impl_t>();
}
template <typename impl_t> attempt_cast_const<std::shared_ptr<impl_t>> Sequence::tryItemAsShared(int index) const {
  return _items[index]->tryAsShared<impl_t>();
}

template <typename impl_t> std::shared_ptr<impl_t> Group::itemAsShared(int index) {
  return _items[index]->asShared<impl_t>();
}

template <typename impl_t> std::shared_ptr<impl_t> NOrMore::itemAsShared(int index) {
  return _items[index]->asShared<impl_t>();
}

template <typename impl_t> std::shared_ptr<impl_t> Optional::asShared() {
  return _subitem->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> OneOf::asShared() {
  return _selected->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> Proxy::asShared() {
  return _selected->asShared<impl_t>();
}
template <typename impl_t> std::shared_ptr<impl_t> Proxy::followAsShared() {
  if (auto as_proxy = _selected->tryAsShared<Proxy>()) {
    return as_proxy.value()->followAsShared<impl_t>();
  }
  return _selected->asShared<impl_t>();
}

///////////////////////////////////////////////////////////////////////////////

 template <typename T> matcher_ptr_t Parser::matcherForTokenClass(T tokclass, std::string name) {
    return matcherForTokenClassID(uint64_t(tokclass), name);
  }

///////////////////////////////////////////////////////////////////////////////
} //namespace ork {
///////////////////////////////////////////////////////////////////////////////
