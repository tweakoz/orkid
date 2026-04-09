---
name: orkcore-reflection
description: Answer questions about orkid's C++ reflection system, RTTI, property registration, serialization, Object class hierarchy, signals/slots, and object factories. Use when the user asks about class registration, property types, serialize/deserialize, or the reflect namespace.
user-invocable: false
---

# Orkid Core Reflection & Object System Reference

When answering questions about reflection, serialization, or the object model in orkid, consult the source files listed below.

## Key Files

| Component | Header | Implementation |
|-----------|--------|----------------|
| RTTI Macros | `ork.core/inc/ork/rtti/RTTI.h` | |
| Object Base | `ork.core/inc/ork/object/Object.h` | `ork.core/src/object/Object.cpp` |
| ObjectClass | `ork.core/inc/ork/object/ObjectClass.h` | `ork.core/src/object/ObjectClass.cpp` |
| Description | `ork.core/inc/ork/reflect/Description.h` | |
| Property Base | `ork.core/inc/ork/reflect/properties/ObjectProperty.h` | |
| Property Registration | `ork.core/inc/ork/reflect/properties/register.h` | |
| Direct Properties | `ork.core/inc/ork/reflect/properties/DirectTyped*.h` | |
| Accessor Properties | `ork.core/inc/ork/reflect/properties/AccessorTyped*.h` | |
| JSON Serializer | `ork.core/inc/ork/reflect/serialize/JsonSerializer.h` | |
| JSON Deserializer | `ork.core/inc/ork/reflect/serialize/JsonDeserializer.h` | |
| SerDes Node Model | `ork.core/inc/ork/reflect/serialize/serdes.h` | |
| Functors | `ork.core/inc/ork/reflect/Functor.h` | |
| Signals/Slots | `ork.core/inc/ork/object/connect.h` | |
| Reflection Init | `ork.core/src/reflect/reflection_init.cpp` | |

## Architecture Overview

### Class Declaration
```cpp
class MyClass : public ork::Object {
  RttiDeclareConcrete(MyClass, ork::Object);  // or RttiDeclareAbstract
  static void Describe();  // Registration entry point
  float _value;
};
```

### Property Registration (in Describe())
```cpp
void MyClass::Describe() {
  auto clazz = GetClassStatic();
  clazz->directProperty("value", &MyClass::_value);           // Direct member
  clazz->accessorProperty("x", &MyClass::getX, &MyClass::setX); // Getter/setter
  clazz->directEnumProperty("mode", &MyClass::_mode);         // Enum
  clazz->directMapProperty("items", &MyClass::_items);         // Map
  clazz->directObjectProperty("child", &MyClass::_child);     // Object ptr
}
```

### Property Type Hierarchy
```
ObjectProperty (base)
+-- ITyped<T> (float, int, string, vec2/3/4, matrix, etc.)
+-- IObject (Object pointers)
+-- IArray (DirectTypedArray, DirectTypedVector, AccessorTypedArray)
+-- IMap (DirectTypedMap, AccessorTypedMap)
+-- DirectEnum<T> (enum values)
```

### Annotations
```cpp
clazz->floatProperty("alpha", float_range{0.0f, 1.0f}, &MyClass::_alpha);
// Editor annotations:
->annotate("editor.filebase", "<assetcache>")
->annotate("editor.filetype", "glb,gltf")
->annotate("editor.browsetype", "folder")
```

### Serialization
- JSON serializer/deserializer via `reflect::serdes::JsonSerializer`
- Properties auto-serialize based on registration
- Lifecycle hooks: `preSerialize()`, `postSerialize()`, `preDeserialize()`, `postDeserialize()`
- Deep copy via `Object::clone()`

### Signals & Slots
- `Signal` — event emitter, registered via `RegisterSignal`
- `AutoSlot` — auto-connected receiver, registered via `RegisterSlot`
- `LambdaSlot` — lambda-based slot

## How to Answer

1. For property registration: check `ObjectClass.h` for `directProperty`, `accessorProperty` etc.
2. For serialization: read `serdes.h` for the node model, `JsonSerializer.h` for JSON format
3. For RTTI macros: check `RTTI.h` — `RttiDeclareConcrete` vs `RttiDeclareAbstract`
4. For annotations: check how property_sheet.cpp reads `editor.*` annotations
