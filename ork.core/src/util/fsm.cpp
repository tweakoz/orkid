////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Hierarchical Finite State Machine
///////////////////////////////////////////////////////////////////////////////

#include <ork/util/fsm.h>
#include <ork/kernel/Array.hpp>

namespace ork::fsm {

///////////////////////////////////////////////////////////////////////////////
// State
///////////////////////////////////////////////////////////////////////////////

State::State(FsmData* data, state_ptr_t parent, std::string name)
    : _parent(parent)
    , _name(name)
    , _data(data) {
}

///////////////////////////////////////////////////////////////////////////////
// LambdaState
///////////////////////////////////////////////////////////////////////////////

LambdaState::LambdaState(FsmData* data, state_ptr_t parent, std::string name)
    : State(data, parent, name)
    , _onenter(nullptr)
    , _onexit(nullptr)
    , _onupdate(nullptr) {
}

void LambdaState::onEnter(fsminstance_ptr_t inst) {
  if (_onenter) {
    _onenter(inst);
  }
}

void LambdaState::onExit(fsminstance_ptr_t inst) {
  if (_onexit) {
    _onexit(inst);
  }
}

void LambdaState::onUpdate(fsminstance_ptr_t inst) {
  if (_onupdate) {
    _onupdate(inst);
  }
}

///////////////////////////////////////////////////////////////////////////////
// FsmData
///////////////////////////////////////////////////////////////////////////////

FsmData::FsmData() {
}

FsmData::~FsmData() {
}

void FsmData::addState(state_ptr_t state) {
  state->_index = _statesByIndex.size();
  _statesByIndex.push_back(state);
  _stateset.insert(state);
}

state_ptr_t FsmData::findState(const std::string& name) const {
  for (const auto& state : _stateset) {
    if (state->_name == name) {
      return state;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void FsmData::addTransition(state_ptr_t from, fsm_event_t event, state_ptr_t to) {
  assert(_stateset.find(from) != _stateset.end());
  assert(_stateset.find(to) != _stateset.end());

  PredicatedTransition pt(to);
  pt._event_token = event;
  from->_transitions[event] = pt;
}

void FsmData::addTransition(state_ptr_t from, fsm_event_t event, const PredicatedTransition& p) {
  assert(_stateset.find(from) != _stateset.end());
  assert(_stateset.find(p._destination) != _stateset.end());

  PredicatedTransition pt = p;
  pt._event_token = event;
  from->_transitions[event] = pt;
}

void FsmData::addTransition(state_ptr_t from, const std::string& event_name, state_ptr_t to) {
  fsm_event_t event = CrcString(event_name.c_str()).hashed();
  PredicatedTransition pt(to);
  pt._event_token = event;
  pt._event_name = event_name;
  from->_transitions[event] = pt;
}

void FsmData::addTransition(state_ptr_t from, const std::string& event_name, const PredicatedTransition& p) {
  fsm_event_t event = CrcString(event_name.c_str()).hashed();
  PredicatedTransition pt = p;
  pt._event_token = event;
  pt._event_name = event_name;
  from->_transitions[event] = pt;
}

///////////////////////////////////////////////////////////////////////////////
// FsmInstance
///////////////////////////////////////////////////////////////////////////////

fsminstance_ptr_t FsmInstance::create(fsmdata_ptr_t data) {
  return std::make_shared<FsmInstance>(data);
}

FsmInstance::FsmInstance(fsmdata_ptr_t data)
    : _data(data)
    , _current(nullptr)
    , _vars(std::make_shared<varmap::VarMap>()) {
}

FsmInstance::~FsmInstance() {
  if (auto grp = _group.lock()) {
    grp->_unregister(this);
  }
}

state_ptr_t FsmInstance::currentState() const {
  std::lock_guard<std::mutex> lock(_currentMutex);
  return _current;
}

///////////////////////////////////////////////////////////////////////////////

void FsmInstance::changeState(state_ptr_t target) {
  ChangeStateEvent cse;
  cse._next = target;
  _pendingEvents.push(cse);
}

void FsmInstance::sendEvent(fsm_event_t event) {
  TokenEvent te(event);
  _pendingEvents.push(te);
}

void FsmInstance::sendEvent(const std::string& event_name) {
  fsm_event_t event = CrcString(event_name.c_str()).hashed();
  TokenEvent te(event);
  _pendingEvents.push(te);
}

void FsmInstance::setInitialState(state_ptr_t state) {
  // Directly set _current without queuing or firing callbacks
  // This is the "birth state" - the FSM is created IN this state
  std::lock_guard<std::mutex> lock(_currentMutex);
  _current = state;
}

///////////////////////////////////////////////////////////////////////////////

void FsmInstance::_performStateChange(fsminstance_ptr_t inst, state_ptr_t to) {
  auto& stateset = inst->_data->states();
  assert((to == nullptr) || (stateset.find(to) != stateset.end()));

  //////////////////////////////////////////////////
  // collect exit handlers from leaf to root
  //////////////////////////////////////////////////

  ork::fixedvector<state_ptr_t, 8> exit_vect;
  ork::fixedvector<state_ptr_t, 8> enter_vect;

  auto exit_collect = [&]() {
    state_ptr_t walk = inst->_current;
    while (walk != nullptr) {
      exit_vect.push_back(walk);
      walk = walk->_parent;
    }
  };

  //////////////////////////////////////////////////
  // collect enter handlers from leaf to root
  //////////////////////////////////////////////////

  auto enter_collect = [&]() {
    state_ptr_t walk = to;
    while (walk != nullptr) {
      enter_vect.push_back(walk);
      walk = walk->_parent;
    }
  };

  //////////////////////////////////////////////////

  if (to == inst->_current) {
    // do nothing
  } else if (nullptr == inst->_current) {
    // nothing to exit
    enter_collect();
  } else if (nullptr == to) {
    // nothing to enter
    exit_collect();
  } else {
    // full hierarchical exit/enter
    exit_collect();
    enter_collect();
  }

  //////////////////////////////////////////////////
  // run collected exit handlers
  //////////////////////////////////////////////////

  for (auto state : exit_vect) {
    // only run if not present in enter_vect
    bool should_run = true;
    for (auto enter_state : enter_vect) {
      if (enter_state == state) {
        should_run = false;
        break;
      }
    }
    if (should_run) {
      state->onExit(inst);
    }
  }

  //////////////////////////////////////////////////
  // run collected enter handlers (root to leaf)
  //////////////////////////////////////////////////

  for (auto it = enter_vect.rbegin(); it != enter_vect.rend(); it++) {
    state_ptr_t state = *it;

    // only run if not present in exit_vect
    bool should_run = true;
    for (auto exit_state : exit_vect) {
      if (exit_state == state) {
        should_run = false;
        break;
      }
    }
    if (should_run) {
      state->onEnter(inst);
    }
  }

  //////////////////////////////////////////////////

  {
    std::lock_guard<std::mutex> lock(inst->_currentMutex);
    inst->_current = to;
  }

  // Notify group of state change (for waitForAllInState)
  if (auto grp = inst->_group.lock()) {
    grp->_notifyStateChange();
  }
}

///////////////////////////////////////////////////////////////////////////////

void FsmInstance::update(fsminstance_ptr_t inst) {
  svar16_t ev;
  while (inst->_pendingEvents.try_pop(ev)) {
    if (ev.isA<ChangeStateEvent>()) {
      const auto& cse = ev.get<ChangeStateEvent>();
      _performStateChange(inst, cse._next);
    } else if (ev.isA<TokenEvent>()) {
      if (inst->_current) {
        const auto& te = ev.get<TokenEvent>();
        auto it = inst->_current->_transitions.find(te._token);
        if (it != inst->_current->_transitions.end()) {
          const PredicatedTransition& trans = it->second;
          if (trans._predicate(inst)) {
            _performStateChange(inst, trans._destination);
          }
        }
      }
    }
  }
  if (inst->_current) {
    inst->_current->onUpdate(inst);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DOT graph generator
///////////////////////////////////////////////////////////////////////////////

std::string FsmData::generateDot(const DotConfig& config, state_ptr_t current) const {
  std::string out;
  out += "digraph " + (config.graph_name.empty() ? "FSM" : config.graph_name) + " {\n";
  out += "  rankdir=LR;\n";
  out += "  bgcolor=\"#1e1e2e\";\n";
  out += FormatString("  sep=\"+%.0f,%.0f\";\n", config.sep, config.sep);
  out += FormatString("  splines=%s;\n", config.splines ? "true" : "false");
  out += FormatString("  K=%.1f;\n", config.K);
  out += FormatString("  overlap=%s;\n", config.overlap ? "true" : "false");
  out += FormatString("  size=\"%.0f,%.0f\";\n", config.size_w, config.size_h);
  out += FormatString("  dpi=%d;\n", config.dpi);
  out += "  node [style=filled, fontname=\"Arial\"];\n";
  out += "  edge [fontname=\"Arial\", fontsize=10, color=\"#f9e2af\"];\n";
  out += "\n";

  // Build parent->children map for clustering
  std::unordered_map<State*, std::vector<state_ptr_t>> children_map;
  std::vector<state_ptr_t> root_states;

  for (const auto& s : _stateset) {
    if (s->_parent) {
      children_map[s->_parent.get()].push_back(s);
    } else {
      root_states.push_back(s);
    }
  }

  // Color palette for hierarchy levels (Catppuccin-inspired)
  struct LevelColors {
    const char* border;
    const char* bg;
    const char* node;
  };
  std::vector<LevelColors> level_colors = {
    {"#89b4fa", "#313244", "#585b70"},  // Level 0: Blue
    {"#cba6f7", "#352f44", "#4e4865"},  // Level 1: Mauve
    {"#94e2d5", "#2d3d3a", "#4a6b66"},  // Level 2: Teal
    {"#fab387", "#3d3230", "#6b5a52"},  // Level 3: Peach
    {"#f5c2e7", "#3d3040", "#6b5268"},  // Level 4: Pink
    {"#f9e2af", "#3a3730", "#635c4a"},  // Level 5: Yellow
  };

  // Recursive lambda to emit clusters
  int cluster_id = 0;
  std::function<void(state_ptr_t, int)> emit_state;
  emit_state = [&](state_ptr_t state, int depth) {
    std::string indent(depth * 2, ' ');
    auto it = children_map.find(state.get());
    bool has_children = (it != children_map.end() && !it->second.empty());

    int level = depth / 2;
    const auto& colors = level_colors[level % level_colors.size()];

    if (has_children) {
      out += indent + "  subgraph cluster_" + std::to_string(cluster_id++) + " {\n";
      out += indent + "    label=\"" + state->_name + "\";\n";
      out += indent + "    style=rounded;\n";
      out += indent + "    color=\"" + std::string(colors.border) + "\";\n";
      out += indent + "    fontcolor=\"" + std::string(colors.border) + "\";\n";
      out += indent + "    bgcolor=\"" + std::string(colors.bg) + "\";\n";

      for (const auto& child : it->second) {
        emit_state(child, depth + 2);
      }

      out += indent + "  }\n";
    } else {
      std::string node_id = "s" + std::to_string(reinterpret_cast<uintptr_t>(state.get()));
      bool is_current = (current == state);
      std::string fillcolor = colors.node;
      std::string fontcolor = "#cdd6f4";
      std::string peripheries = is_current ? ", peripheries=2" : "";
      std::string penwidth = is_current ? ", penwidth=2" : "";
      out += indent + "  " + node_id + " [label=\"" + state->_name + "\", fillcolor=\"" + fillcolor + "\", fontcolor=\"" + fontcolor + "\", color=\"" + std::string(colors.border) + "\"" + peripheries + penwidth + ", shape=rect, style=\"filled,rounded\", margin=\"0.11,0.03\"];\n";
    }
  };

  for (const auto& root : root_states) {
    emit_state(root, 0);
  }

  out += "\n  // Transitions\n";

  // Edge colors
  std::vector<const char*> edge_colors = {
    "#f9e2af", "#a6e3a1", "#89dceb", "#f5c2e7", "#fab387", "#cba6f7",
    "#94e2d5", "#eba0ac", "#74c7ec", "#f2cdcd", "#b4befe", "#f5e0dc",
    "#89b4fa", "#a6adc8", "#bac2de", "#cdd6f4", "#e6c384", "#7aa2f7",
    "#9ece6a", "#ff9e64", "#bb9af7", "#7dcfff", "#c0caf5", "#e0af68",
  };

  std::unordered_map<std::string, size_t> event_color_map;
  size_t next_color = 0;

  for (const auto& state : _stateset) {
    auto it = children_map.find(state.get());
    bool has_children = (it != children_map.end() && !it->second.empty());
    if (has_children) continue;

    std::string from_id = "s" + std::to_string(reinterpret_cast<uintptr_t>(state.get()));

    for (const auto& [event, trans] : state->_transitions) {
      if (trans._destination) {
        auto dest_it = children_map.find(trans._destination.get());
        bool dest_has_children = (dest_it != children_map.end() && !dest_it->second.empty());
        if (dest_has_children) continue;

        std::string to_id = "s" + std::to_string(reinterpret_cast<uintptr_t>(trans._destination.get()));

        std::string label = trans._event_name.empty()
            ? ("0x" + FormatString("%llx", event))
            : trans._event_name;

        auto color_it = event_color_map.find(label);
        if (color_it == event_color_map.end()) {
          event_color_map[label] = next_color;
          next_color = (next_color + 1) % edge_colors.size();
        }
        const char* edge_color = edge_colors[event_color_map[label]];

        out += "  " + from_id + " -> " + to_id;
        out += " [label=\"" + label + "\", color=\"" + edge_color + "\", fontcolor=\"" + edge_color + "\"];\n";
      }
    }
  }

  out += "}\n";
  return out;
}

std::string FsmInstance::generateDot(const DotConfig& config) const {
  return _data->generateDot(config, _current);
}

///////////////////////////////////////////////////////////////////////////////
// FsmGroup
///////////////////////////////////////////////////////////////////////////////

fsminstance_ptr_t FsmGroup::createInstance(fsmgroup_ptr_t group, fsmdata_ptr_t data) {
  auto inst = std::make_shared<FsmInstance>(data);
  inst->_group = group;
  group->_register(inst.get());
  return inst;
}

void FsmGroup::_register(FsmInstance* inst) {
  std::lock_guard<std::mutex> lock(_mutex);
  _instances.insert(inst);
}

void FsmGroup::_unregister(FsmInstance* inst) {
  std::lock_guard<std::mutex> lock(_mutex);
  _instances.erase(inst);
}

void FsmGroup::broadcastEvent(fsm_event_t event) {
  std::vector<FsmInstance*> copy;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    copy.assign(_instances.begin(), _instances.end());
  }
  // Send events outside of lock to avoid deadlocks
  for (auto* inst : copy) {
    TokenEvent te(event);
    inst->_pendingEvents.push(te);
  }
}

void FsmGroup::broadcastEvent(const std::string& event_name) {
  fsm_event_t event = CrcString(event_name.c_str()).hashed();
  broadcastEvent(event);
}

bool FsmGroup::allInState(state_ptr_t state) {
  std::lock_guard<std::mutex> lock(_mutex);
  for (auto* inst : _instances) {
    if (inst->currentState() != state) {
      return false;
    }
  }
  return true;
}

bool FsmGroup::allInState(const std::string& state_name) {
  std::lock_guard<std::mutex> lock(_mutex);
  for (auto* inst : _instances) {
    auto cur = inst->currentState();
    if (!cur || cur->_name != state_name) {
      return false;
    }
  }
  return true;
}

void FsmGroup::waitForAllInState(state_ptr_t state) {
  std::unique_lock<std::mutex> lock(_mutex);
  _cv.wait(lock, [this, state]() {
    for (auto* inst : _instances) {
      if (inst->currentState() != state) {
        return false;
      }
    }
    return true;
  });
}

void FsmGroup::waitForAllInState(const std::string& state_name) {
  std::unique_lock<std::mutex> lock(_mutex);
  _cv.wait(lock, [this, &state_name]() {
    for (auto* inst : _instances) {
      auto cur = inst->currentState();
      if (!cur || cur->_name != state_name) {
        return false;
      }
    }
    return true;
  });
}

void FsmGroup::_notifyStateChange() {
  _cv.notify_all();
}

size_t FsmGroup::count() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _instances.size();
}

std::vector<FsmInstance*> FsmGroup::instances() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return std::vector<FsmInstance*>(_instances.begin(), _instances.end());
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::fsm
