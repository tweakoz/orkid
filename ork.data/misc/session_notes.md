# Orkid Engine - LLM Interaction Guide

## Document Purpose & Evolution

This living document guides LLMs in working with the Orkid game engine codebase. It captures architectural patterns, coding conventions, and interaction preferences learned through collaborative development sessions.

### Self-Improvement Protocol

When working with this codebase, the LLM should:
1. **Capture New Patterns**: When discovering recurring generic patterns not documented here, request to user to add them here
2. **Refine Existing Guidance**: If instructions prove ambiguous or incomplete, clarify them
3. **Document Gotchas**: When encountering non-obvious issues, document them for future sessions
4. **Update Examples**: Replace outdated examples with current, working code snippets
5. **Track Design Decisions**: When architectural decisions are made, record the reasoning

> **Meta-Rule**: This document should grow more valuable with each session. Try when applicable to leave it better than you found it.

## Critical Design Patterns - Prime Directives

### 1. NO shared_from_this Pattern (NEVER VIOLATE)

This is THE fundamental pattern across the entire Orkid codebase. When forming hierarchies with `shared_ptr`:

```cpp
// CORRECT - Static factory pattern
struct Parent;
struct Child;

using parent_ptr_t = std::shared_ptr<Parent>;
using child_ptr_t = std::shared_ptr<Child>;

struct Parent {
    // Static factory receives parent as shared_ptr
    static child_ptr_t createChild(parent_ptr_t self, /* other args */) {
        auto child = std::make_shared<Child>();
        child->_parent = self;  // Child stores weak_ptr or raw ptr (see note below)
        return child;
    }
};

struct Child {
    std::weak_ptr<Parent> _parent;  // weak_ptr if lifetime is uncertain and you made need to check null
    // Parent* _parent;             // raw ptr preferred when parent is guaranteed to outlive child
};
```

**Raw pointer vs weak_ptr for back-references:** Use raw pointers in hot-path code (rendering, per-frame updates, audio) where the lifetime of the pointed-to object is well known and guaranteed. Use `weak_ptr` when the referenced object could genuinely become null during the code's lifetime and you need to check for that — `lock()` gives you a safe nullable handle.

**Why This Matters:**
- Prevents circular reference memory leaks
- Makes ownership explicit and debuggable
- Follows RAII principles correctly
- Enables clear parent-child relationships

**Apply This Pattern To:**
- Any parent-child relationship
- Factory methods
- Builder patterns
- Component systems
- Scene graphs

### 2. Type System Conventions

All types follow this pattern in `types.h`:

```cpp
// Forward declaration
struct MyType;

// Pointer type aliases
using mytype_ptr_t = std::shared_ptr<MyType>;
using mytype_constptr_t = std::shared_ptr<const MyType>;
using mytype_wkptr_t = std::weak_ptr<MyType>;  // Only if needed

// Collection type aliases  
using mytype_vect_t = std::vector<mytype_ptr_t>;
using mytype_set_t = std::set<mytype_ptr_t>;
```

**Rules:**
- ALWAYS use type aliases, never raw `std::shared_ptr<X>`
- Forward declare in appropriate `types.h`
- Keep pointer suffix consistent: `_ptr_t`, `_constptr_t`, `_wkptr_t`
- Collection types use `_vect_t`, `_set_t`, `_map_t` suffixes

### 3. Builder & Factory Patterns

Parents create children, children never create themselves:

```cpp
// CORRECT
auto child = Parent::createChild(parent_ptr, id, version);
auto grandchild = Child::createGrandchild(child_ptr, id, priority);

// WRONG  
auto child = std::make_shared<Child>(id, version);
auto grandchild = child->createGrandchild(id, priority);  // Non-static factory
```

### 4. Flyweight Pattern Usage

For objects that should have single instances (namespaces, requests, resource handles, etc.):

```cpp
// Merge pattern - get existing or create new
resource_ptr_t mergeResource(const std::string& id) {
    auto it = _resources.find(id);
    if (it != _resources.end()) {
        return it->second;  // Return existing
    }
    // Create new with proper hierarchy
    auto resource = std::make_shared<Resource>(id);
    _resources[id] = resource;
    return resource;
}
```

## Code Organization

### Directory Structure

| Component | Location | Purpose |
|-----------|----------|---------|
| Source files | `ork.core/src` | Implementation files |
| Headers | `ork.core/inc/ork/` | Public headers |
| Python bindings | `ork.core/pyext/` | pybind11 bindings |
| Python tests | `ork.core/pyext/tests/` | Python unit tests |
| C++ tests | `ork.core/tests/` | UnitTest++ tests |
| Documentation | `ork.dox/` | System documentation |
| Data files | `ork.data/` | Runtime data, configs |

### Module Organization

Each subsystem follows this pattern:
```
ork.core/inc/ork/subsystem/
├── types.h          # Forward declarations & type aliases
├── main_class.h     # Primary interface
├── support_class.h  # Supporting classes
└── implementation.h # Implementation details (if needed)

ork.core/src/subsystem/
├── main_class.cpp
├── support_class.cpp  
└── subsystem_specific.cpp
```

## Development Workflow

### Building

```bash
# Standard build (Release mode, parallel)
ork.build.py

# The build system handles:
# - CMake configuration
# - Dependency management  
# - Installation to <stage>
# - Python module deployment
```

**Build System Notes:**
- Custom system provided by obt package
- Don't timeout builds - they can be slow if core headers changed (>300s)
- Build artifacts go to `<stage>/builds/orkid/.build`
- Installs to `<stage>` , aka ${OBT_STAGE}

# OBT (Orkid Build Tools) 
- base layer for orkid build system (and other projects as well)
- is pip installed into base level venv via pip3 install ork.build # its on pypi 
- has base level commands like obt.dep.build.py obt.dep.list.py 
- has obt python modules with helpers for path anchoring, manipulation
- has build system abstraction layer for cmake, GNU automake, configure, meson build, boost jam etc..
- has 'depper modules' which build software packages, typically from source on github
- has 'docker modules' for building, managing docker containers, composed containers with automatic environment variable manipulation, etc..
- can be extended with plugin system from projects (triggered when a project repo has an obt.project folder). orkid is one of those projects.
- uses 'reverse dns' like notation for commands in path. allows user to do filtered search using base.tabtab completion. obt uses obt. prefix. orkid uses ork. prefix.

### Testing

#### C++ Tests
```bash
# Run tests matching prefix
ork.test.core.exe TestPrefix!

# Examples
ork.test.core.exe Texture!    # All texture tests
ork.test.core.exe Shader!     # Just shader tests
```

**Test Rules:**
- Wildcard is `!` (not `*`) to avoid shell expansion
- Wildcard only at end
- No additional arguments
- Test binary is in `$PATH`

#### Python Tests
```bash
cd ork.core/pyext/tests
./test_name.py
```

**Python Requirements:**
- MUST use `#!/usr/bin/env ork.python` shebang
- Uses custom Python environment
- Tests should be incremental
- Verify each step before proceeding

## Coding Standards

### General Principles

1. **No Faking**: Real implementations only. If stuck, defer to user.
2. **Direct Style**: No fluff, straight to the point
3. **Full Understanding**: Read entire subsystems before modifying
4. **Incremental Development**: Each change must work before proceeding

## Code Search Strategy Guide

### Tool Hierarchy & When to Use Each

**The Golden Rule**: Start broad with database tools, then narrow with text search.

#### 1. C++ Database Tools (ork.cpp.*) - USE FIRST for structural queries
These tools use a pre-built SQLite database of parsed C++ entities. They understand C++ semantics.

```bash
# When you need to understand CLASS/STRUCT structure
ork.cpp.search.py Camera              # Find all entities with "Camera" 
ork.cpp.search.py -t class UiCamera   # Find specific class
ork.cpp.search.py -t objects Camera   # Find both classes AND structs
ork.cpp.members.py CameraData         # See all members of a type

# When you need to trace REFERENCES
ork.cpp.references.py "ork::lev2::Context"                    # Find all uses
ork.cpp.references.py "ork::lev2::CameraMatrices::_frustum"   # Find field accesses

# When you need INHERITANCE information  
ork.cpp.inhtree.py "ork::lev2::Context"   # See base and derived classes

# When you need ENUM information
ork.cpp.enums.py EBufferFormat            # See enum values and CRC hashes
ork.cpp.enums.py --hash 0xE15695B7        # Reverse lookup hash to enum value
```

**Critical Learning**: These tools require EXACT names or patterns. If searching fails:
- Drop namespace qualifiers and search broadly first
- Use the short name (Camera not ork::lev2::Camera) to find the canonical name
- The tools will show you the full qualified names in results

#### 2. Text Search Tools - USE SECOND for implementation details

**ork.find.py - Your Primary Text Search Tool**
```bash
# Basic search - finds in all orkid modules
ork.find.py LoadTexture                   # Find all occurrences
ork.find.py "LoadTexture("                # Find function calls specifically
ork.find.py LoadTexture | grep cpp        # Filter to .cpp files only
ork.find.py LoadTexture | grep "\.h:"     # Filter to headers only

# Finding specific patterns
ork.find.py "shared_from_this"            # Find anti-patterns
ork.find.py "_contentHash ="              # Find assignments
ork.find.py "TextureInterface::"          # Find class method implementations
```

**Grep Tool - When you need regex or context**
```bash
# When you need context lines
Grep -B 3 -A 3 "LoadTexture" --output_mode=content   # Show 3 lines before/after

# When you need file lists
Grep "CrcEnum" --output_mode=files_with_matches      # Just show which files contain pattern

# Complex regex patterns
Grep "enum\s+(class|struct)" --output_mode=content   # Find enum declarations
```

#### 3. File Tools - For exploring and reading

```bash
# List files in database
ork.cpp.db.files.py list --limit 20       # See what files are indexed

# Search for files
ork.cpp.db.files.py search "*.cpp"        # Find all cpp files
ork.cpp.db.files.py find txi.cpp          # Find specific file

# Show file content
ork.cpp.db.files.py show txi.cpp          # Display file with line numbers
```

### Real-World Search Patterns

#### Pattern 1: "I need to understand how a class works"
```bash
# Step 1: Find the class
ork.cpp.search.py TextureInterface

# Step 2: See its members
ork.cpp.members.py ork::lev2::TextureInterface

# Step 3: Find its implementation
ork.find.py "TextureInterface::" | grep cpp

# Step 4: See how it's used
ork.cpp.references.py "ork::lev2::TextureInterface"
```

#### Pattern 2: "I need to find where something is defined"
```bash
# For types/classes - use database first
ork.cpp.search.py EBufferFormat

# For functions - use text search
ork.find.py "LoadTexture.*\{"     # Find function definitions
ork.find.py "def LoadTexture"     # Find Python definitions
```

#### Pattern 3: "I need to understand an enum and its values"
```bash
# See the enum definition and values
ork.cpp.enums.py EBufferFormat

# Find where it's used
ork.find.py "EBufferFormat::"

# Reverse lookup a hash from logs
ork.cpp.enums.py --hash 0xE15695B7
```

#### Pattern 4: "I need to trace through a codebase"
```bash
# Start with high-level search
ork.cpp.search.py -t objects Texture

# Pick interesting class, see members
ork.cpp.members.py ork::lev2::Texture

# Find specific member usage
ork.cpp.references.py "ork::lev2::Texture::_contentHash"

# Read the actual implementation
ork.find.py "_contentHash =" | head -20
```

### Common Pain Points & Solutions

**Pain Point 1: "No results found" with database tools**
```bash
# DON'T do this:
ork.cpp.search.py ork::lev2::Camera  # Too specific, might fail

# DO this instead:
ork.cpp.search.py Camera              # Search broadly first
ork.cpp.search.py -t objects Camera   # Then narrow by type
```

**Pain Point 2: Need to see actual code, not just references**
```bash
# DON'T rely only on database tools
ork.cpp.references.py "LoadTexture"   # Only shows locations

# DO combine with text search
ork.find.py "LoadTexture" | head -20  # See actual code immediately
```

**Pain Point 3: Too many results**
```bash
# DON'T do this:
ork.find.py Context                   # Too broad, hundreds of results

# DO refine your search:
ork.find.py "class Context"           # More specific
ork.find.py "Context::" | grep cpp    # Method implementations only
ork.cpp.search.py -t class Context    # Use database for structured search
```

**Pain Point 4: Not sure of exact name**
```bash
# Use wildcards and patterns
ork.cpp.search.py "*Camera*"          # Database search with wildcards
ork.find.py "[Cc]amera"                # Text search with regex
ork.cpp.enums.py "ork::lev2::*"       # All enums in namespace
```

### Search Strategy Flowchart

```
Need to find something?
├── Is it a C++ entity (class/struct/enum)?
│   ├── YES → Start with ork.cpp.search.py
│   │   ├── Found it? → Use ork.cpp.members.py for details
│   │   └── Not found? → Try without namespace, use wildcards
│   └── NO → Use ork.find.py
│
├── Need to understand relationships?
│   ├── Inheritance → ork.cpp.inhtree.py
│   ├── References → ork.cpp.references.py
│   └── Dependencies → Combine both
│
├── Need implementation details?
│   ├── Single file → Read tool or ork.cpp.db.files.py show
│   └── Multiple files → ork.find.py with grep filtering
│
└── Debugging enum/hash issues?
    └── ork.cpp.enums.py (with --hash for reverse lookup)
```

### Pro Tips from Experience

1. **Build the database first**: Before any C++ analysis session, ensure database is current:
   ```bash
   ork.cpp.db.build.py -m core lev2  # Build for modules you'll analyze
   ```

2. **Use JSON output for complex analysis**: Database tools support --json for scripting:
   ```bash
   ork.cpp.search.py Camera --json | jq '.entities[].canonical_name'
   ```

3. **Combine tools in pipelines**:
   ```bash
   # Find all classes that inherit from Object
   ork.cpp.search.py -t class --all --json | \
     jq -r '.entities[].canonical_name' | \
     xargs -I{} ork.cpp.inhtree.py {} 2>/dev/null | \
     grep "ork::Object"
   ```

4. **Remember the colorization in ork.find.py output**:
   - Yellow `[38;5;228m` = filenames (easy to spot file boundaries)
   - Gold `[38;5;226m` = line numbers (for navigation)
   - Context after line number is actual code

5. **When in doubt, start broad**: It's easier to filter down than to guess exact names:
   ```bash
   ork.cpp.search.py Texture           # Start here
   ork.cpp.search.py -t class Texture  # Then narrow
   ork.cpp.search.py -t class -n ork::lev2 Texture  # Then filter more
   ```

**Statistical trigger**: The colorized file:line:content format with ANSI codes provides superior information extraction speed compared to native Grep tool which requires multiple calls and manual correlation.

### Command Naming Convention - Reverse DNS Notation

All Python commands in obt and orkid follow reverse DNS notation (generic→specific):

```bash
# Pattern: ork.category.subcategory.specific.py
ork.build.py                    # Generic build command
ork.test.core.exe              # Test runner for core
ork.llm.read.log.py            # LLM log reader
ork.asset.catalog.fetch.py     # Asset catalog fetch tool
ork.asset.catalog.list.py      # Asset catalog listing
```

**Why This Matters:**
- **Cognitive Load Reduction**: Commands group naturally in filesystem/tab completion
- **Discoverability**: `ork.<TAB>` shows all orkid commands
- **Hierarchy Clear**: `ork.asset.<TAB>` shows all asset-related tools
- **Consistency**: Same pattern as C++ namespaces (`ork::subsystem::class`)
- **Muscle Memory**: Predictable patterns reduce mental overhead

**Apply This Pattern To:**
- New command-line tools
- Python scripts in obt.project/bin
- Any user-facing commands

### Comment Separators

Full-width (`////////////////////////////////////////////////////////////////////////////////`) separators are used between top-level declarations and method definitions. Half-width (`////////////////////////////////////////`) separators are used within method bodies to divide logical chunks of work. Named half-width blocks annotate significant categories of work within a method.

**Header example:**
```cpp
////////////////////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////////////////////

struct CurrentState {
  int _variable_name;
  void methodName(int param_name);
  void otherMethodName(int param_name);
};

////////////////////////////////////////////////////////////////////////////////

struct NextState {
  int _variable_name;
  void methodName(int param_name);
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ork
////////////////////////////////////////////////////////////////////////////////
```

**Implementation example:**
```cpp
////////////////////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////////////////////

void CurrentState::methodName(int param_name) {
  // minor comment
  ** logical chunk of work **

  ////////////////////////////////////////

  // minor comment
  ** next logical chunk of work **

  ////////////////////////////////////////
  // Significant Named Category of Work
  ////////////////////////////////////////

  // minor comment
  ** logical chunk of work **
}

void CurrentState::otherMethodName(int param_name) {
  // minor comment
  ** logical chunk of work **
}

////////////////////////////////////////////////////////////////////////////////

void NextState::methodName(int param_name) {

  ////////////////////////////////////////
  // Significant Named Category of Work
  //  Note about category.
  ////////////////////////////////////////

  // minor comment
  ** logical chunk of work **
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork
////////////////////////////////////////////////////////////////////////////////
```

### C++ Conventions

Always use `struct`, never `class`. All members are public by default — this is intentional. Access control is enforced by convention (underscore prefix for internal methods/members) not by language enforcement.

```cpp
// Member variables with underscore prefix
struct MyStruct; // fwd decl (above definition or in types.h)
using mystruct_ptr_t = std::shared_ptr<MyStruct>; 
using mystruct_wkptr_t = std::weak_ptr<MyStruct>;  // only when really needed

struct MyStruct {
  // all public 
  // methods up top
  void publicMethod();                                       // Camel case for methods
  void _internalMethodWithPublicInterfaceExposure();         // _ denotes internal on methods
  virtual ~MyStruct();                                       // only when subclassed and used polymorphically                      
  static void noSharedFromThis(mystruct_ptr_t self);         // when class needs a shared_ptr of itself   
  static void noSharedFromThis2(mystruct_ptr_t self,int arg);// .. and needs other args
  // variables down low  
  int _member_variable = 0;                                  // prefer pod init in header
  int _member_variable2 = 0;                                 // member variables use lowercase and _ for spacing
  static constexpr int _static_const = 42;                   // use constexpr when appropriate
  static constexpr size_t _static_some_size_or_length = 42;  // use appropiate type for job

};


// Type aliases over raw types
using path_list_t = std::vector<path_t>;      // GOOD (can go in subsystem's types.h if it exists and publically used), reduces cognitive burden when reading...
std::vector<file::Path> paths;                // AVOID
path_list_t paths;                            // GOOD, reduces cognitive burden when reading...

// Namespace usage
namespace ork::subsystem {  // Nested namespaces
    // Implementation
}

using namespace X; // only in cpp files, not headers

namespace ork::subsystem {  // in header
   namespace nsalias = ::some::really::really::long:namespace;  // GOOD
   namespace nsalias2 = ::shortns;  // BAD
   struct X {
    X(nsalias::some_ptr_t s);  // GOOD
    X(nsalias2::some_ptr_t s); // BAD
   };
}

```

### Python Binding Conventions

```cpp
// In pyext_*.cpp files
py::class_<MyStruct, mystruct_ptr_t>(module, "MyStruct")
    .def(py::init<>())
    .def_property_readonly("member_variable", [](mystruct_ptr_t obj) -> int { // member variables use lowercase and _ for spacing
        return obj->_member_variable;
    })
    .def("publicMethod", [](mystruct_ptr_t obj) { // camelCase for method names
        obj->publicMethod();
    })
    .def("publicMethod2", [](mystruct_ptr_t obj, int i) { // no shared_from_this
        MyStruct::noSharedFromThis2(obj,i);
    })
    .def("method", &MyStruct::method, py::arg("param"))
    .def("__repr__", [](mystruct_ptr_t obj) {
        return FormatString("MyStruct(%p)", obj.get());
    });

// Path conversions (takes a variety of types convertable to string as a path)
.def("load", [](MyStruct* self, py::object path) {
    auto as_pystr = py::cast<py::str>(path); // as if str(x) were called
    auto as_str = as_pystr.cast<std::string>();
    return self->load(file::Path(as_str));
})
```

### Error Handling Philosophy

- **Mixed approach**: Some methods return nullptr or error state, others OrkAssert, fail early when appropriate
- **Document expectations**: Make failure modes clear
- **Python bindings**: Convert to exceptions where appropriate
- **Resource management**: RAII everywhere, no manual cleanup

### `_buildup()` / `_teardown()` Pattern

Used for objects that need to be torn down and rebuilt mid-lifetime (e.g. swapchain recreation on window resize). The constructor calls `_buildup()` directly; the destructor calls `_teardown()`.

**Error handling inside `_buildup()`:** Use `OrkAssert` for unrecoverable init failures. Do not throw.

### `throw` vs `OrkAssert` — When to Use Which

**Use `OrkAssert`** for unrecoverable engine/render/vulkan errors — hardware init failures, missing Vulkan functions, GPU allocation failures, invalid internal state. There are no catch blocks in the render pipeline; a `throw` here will call `std::terminate()` anyway, so `OrkAssert` is cleaner and more consistent.

**Use `throw std::runtime_error`** only for errors that propagate to a known catch boundary:
- Asset/catalog I/O operations (caught in `ork.core/src/asset/catalog/`)
- Utility functions called from Python bindings (caught in `pyext_*.cpp`)
- Network/crypto operations (caught in their respective utility wrappers)

## Architecture Principles

### Thread Safety

- **Generation-based versioning**: Increment generation on state changes
- **Lock-free reads**: Where possible using atomic operations  
- **Clear ownership**: One writer, many readers pattern
- **Immutable data**: Prefer immutable structures for shared state

### I/O Preferences

- **Real I/O**: Use stdio, POSIX, boost::filesystem
- **No abstractions**: Avoid FileEnv wrapper until it is refactored.
- **Direct operations**: `fopen`, `fread`, `fwrite` for C-style
- **Boost for paths**: `boost::filesystem` for path manipulation

### Memory Management

- **RAII everywhere**: Constructors acquire, destructors release
- **Smart pointers**: `shared_ptr` for shared ownership, `unique_ptr` for single
- **Weak pointers**: Break cycles, especially parent-child
- **No raw new/delete**: Always use make_shared or make_unique, except in special cases.

## Platform Support

### Target Platforms
- macOS Darwin (primary development)
- Modern Linux (Ubuntu 20.04+, RHEL 8+)
- Windows (experimental)

### Platform Abstractions
- Use POSIX when unsure
- Engine provides cross-platform abstractions
- Prefer standard library over platform-specific code
- Test on multiple platforms regularly

## Documentation Standards

### Code Documentation
```cpp
// File header
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// struct documentation
////////////////////////////////////////////////////////////////////////////////
// Brief description of struct purpose
//
// Detailed explanation including:
// - Key responsibilities
// - Usage patterns  
// - Thread safety guarantees
// - Example code if non-obvious
////////////////////////////////////////////////////////////////////////////////
struct MyStruct {
```

### Maintaining Project Documentation

**Key Documents:**
- `ork.dox/*.md` - System documentation
- `README.md` - Project overview
- This file - LLM interaction guide

**When to Update:**
- New patterns discovered
- API changes
- Architectural decisions
- Build process changes

## Common Pitfalls & Solutions

### Pitfall: Using shared_from_this
**Solution**: Use static factory pattern with parent passed as parameter

### Pitfall: Raw pointer types in APIs
**Solution**: Always use type aliases (`_ptr_t` suffixes)

### Pitfall: Circular dependencies
**Solution**: Forward declarations in types.h, weak_ptr for back-references

### Pitfall: Platform-specific code
**Solution**: Use engine abstractions or POSIX APIs

### Pitfall: Manual memory management  
**Solution**: RAII and smart pointers everywhere

### Pitfall: Over-filtering command output (tail, grep, head)
**Problem**: Filtering build output or test results can hide critical information and create misleading impressions of success. This is especially problematic with parallel builds where errors can appear anywhere in the output stream.

**Examples of Bad Patterns:**
```bash
# BAD: Hides actual build status
ork.build.py 2>&1 | tail -5           # May show "built" targets while hiding errors
ork.build.py 2>&1 | grep "error:"     # Misses context, warnings, and completion status

# BAD: Can miss critical test failures
test.py 2>&1 | tail -20               # May cut off stack traces or setup errors
test.py 2>&1 | grep "PASS"            # Creates false impression of success

# BAD: Loses important diagnostic information  
command 2>&1 | grep -A2 "pattern"     # Arbitrary context windows miss related info
```

**Why This Matters:**
- Build systems interleave output in parallel mode - errors aren't always at the end
- Test failures often have critical context before/after the actual error
- Completion status (make exit codes, final summaries) often appear after errors
- Stack traces and diagnostic messages can span hundreds of lines
- Filtering creates false confidence - seeing "Building [100%]" doesn't mean build succeeded

**Solution**: Always run commands without filters first to understand the full output. Only filter when you know exactly what you're looking for and understand what you might miss. For build errors, search for the FIRST error, not the last lines of output.

## Session Management

### Working with LLMs

1. **Start each session**: Review this document for context
2. **During session**: Update patterns as discovered
3. **End of session**: Document any new findings here
4. **Complex tasks**: Break into smaller, verifiable steps

### Information Preservation

When discovering important patterns or conventions:
1. Add to relevant section above
2. Include concrete example
3. Explain why it matters
4. Note any exceptions

### LLM Log Search (for pattern discovery)

Use `ork.llm.read.log.py` to find previous solutions:

```bash
# List all available sessions
ork.llm.read.log.py --list

# Find recent work
ork.llm.read.log.py --recent

# Search for patterns in a specific file
ork.llm.read.log.py -s "pattern_name" [session-uuid].jsonl

# Time-based search across all files
ork.llm.read.log.py --show "14:00" "16:00" -s "shared_ptr"
```

## Quick Reference Card

### Must Remember
- ❌ NEVER use `shared_from_this`
- ✅ ALWAYS use type aliases (`_ptr_t`)
- ✅ ALWAYS use static factory pattern
- ✅ Parents create children
- ✅ Children store `weak_ptr` to parent
- ✅ Forward declare in types.h
- ✅ One line must work before next

### Build & Test
```bash
ork.build.py                    # Build everything
ork.test.core.exe TestName!     # Run C++ tests  
./test_name.py                  # Run Python tests (from test dir)
```

### Python Requirements
- Shebang: `#!/usr/bin/env ork.python`
- Custom environment, not system Python
- Use `py::arg()` for all parameters
- Convert paths from py::object to file::Path

---

*This is a living document. Each interaction should improve it.*