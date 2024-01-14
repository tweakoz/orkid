# Orkid Reflection System

ork.core includes a built in reflection system facilitating easy editors and serialization / deserialization.


---

## reflection property registration example 

```cpp

#include <ork/rtti/RTTIX.inl>
#include <ork/reflect/properties/registerX.inl>
#include <ork/object/ObjectClass.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>

using namespace ork;
using namespace ork::object;

///////////////////////////////////////////////////////////////////////////////

struct SimpleTest final : public Object {
  DeclareConcreteX(SimpleTest, Object);

public:
  SimpleTest(std::string str = "");
  std::string _strvalue;
};

///////////////////////////////////////////////////////////////////////////////

struct MapTest final : public Object {
  DeclareConcreteX(MapTest, Object);

public:
  MapTest();

  std::map<int, std::string> _directintstrmap;
  std::map<std::string, int> _directstrintmap;
  std::unordered_map<int, std::string> _directintstrumap;
  std::unordered_map<std::string, int> _directstrintumap;
  orklut<std::string, int> _directstrintlut;
  std::map<std::string, simpletest_ptr_t> _directstrobjmap;
};

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(SimpleTest, "SimpleTest");
ImplementReflectionX(MapTest, "MapTest");

///////////////////////////////////////////////////////////////////////////////
void SimpleTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directProperty(
      "value", //
      &SimpleTest::_strvalue);
}
///////////////////////////////////////////////////////////////////////////////
SimpleTest::SimpleTest(std::string str)
    : _strvalue(str) {
}
///////////////////////////////////////////////////////////////////////////////
MapTest::MapTest() {
}
///////////////////////////////////////////////////////////////////////////////
void MapTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directMapProperty(
      "directintstr_map", //
      &MapTest::_directintstrmap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directstrint_map", //
      &MapTest::_directstrintmap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directintstr_unorderedmap", //
      &MapTest::_directintstrumap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directstrint_unordered_map", //
      &MapTest::_directstrintumap);
  ///////////////////////////////////
  auto P1 = clazz->directMapProperty(
      "directstrint_lut", //
      &MapTest::_directstrintlut);
  P1->annotate("editor.visible", false);  // not visible in UI editors
  ///////////////////////////////////
  clazz->directObjectMapProperty(
      "directstrobj_map", //
      &MapTest::_directstrobjmap)        // annotation direct on property declaration
      ->annotate("python.visible",false) // not visible from python bindings
      ->annotate("hello",true);          // continued annotation
  ///////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
std::string maptest_generate() {
  auto maptest                  = std::make_shared<MapTest>();
  maptest->_directintstrmap[1]  = "one";
  maptest->_directintstrmap[2]  = "two";
  maptest->_directintstrmap[3]  = "three";
  maptest->_directintstrmap[42] = "theanswer";

  maptest->_directstrintmap["one"]       = 1;
  maptest->_directstrintmap["two"]       = 2;
  maptest->_directstrintmap["three"]     = 3;
  maptest->_directstrintmap["theanswer"] = 42;

  maptest->_directintstrumap[1]  = "one";
  maptest->_directintstrumap[2]  = "two";
  maptest->_directintstrumap[3]  = "three";
  maptest->_directintstrumap[42] = "theanswer";

  maptest->_directstrintumap["one"]       = 1;
  maptest->_directstrintumap["two"]       = 2;
  maptest->_directstrintumap["three"]     = 3;
  maptest->_directstrintumap["theanswer"] = 42;

  maptest->_directstrintlut.AddSorted("one", 1);
  maptest->_directstrintlut.AddSorted("two", 2);
  maptest->_directstrintlut.AddSorted("three", 3);
  maptest->_directstrintlut.AddSorted("theanswer", 42);

  maptest->_directstrobjmap["yo"]    = std::make_shared<SimpleTest>("one");
  maptest->_directstrobjmap["two"]   = std::make_shared<SimpleTest>("two");
  maptest->_directstrobjmap["three"] = std::make_shared<SimpleTest>("three");

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(maptest);
  return ser.output();
}
///////////////////////////////////////////////////////////////////////////////
int main( int argc, char** argv, char** envp ){

  ///////////////////////////
  // required initialization for apps using reflection
  ///////////////////////////

  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto stringpoolctx = std::make_shared<StringPoolContext>();
  StringPoolStack::Push(stringpoolctx);

  /////////////////////
  // touch class reflection data to prevent stripping linkers from stripping
  /////////////////////
	
  SimpleTest::GetClassStatic(); 
  MapTest::GetClassStatic();
  rtti::Class::InitializeClasses(); // build class tree

  ///////////////////////////
  // serialize object to json
  ///////////////////////////

  auto json = maptest_generate();
  printf( "json:\n%s\n", json.c_str() );

  ///////////////////////////
  // clone object by deserializing the JSON
  //. the clone will have the same UUID's as the original.
  //  TODO: add a clone (with UUID regen option)
  ///////////////////////////

  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(json.c_str());
  deser.deserializeTop(instance_out);
  auto clone = objcast<MapTest>(instance_out);

  ///////////////////////////
  StringPoolStack::Pop();

  return 0;
}

```

* executing the above snippet will result in the following json being produced (with different UUIDs of course)
* Note that Orkid Reflection JSON is human readable (and some might say writable). This is by design - It will come in hella handy when diffing.

```json
{
 "root": {
  "object": {
   "class": "MapTest",
   "uuid": "59488646-160f-4698-9ebf-f449658bb741",
   "properties": {
    "directintstr_map": {
     "1": "one",
     "2": "two",
     "3": "three",
     "42": "theanswer"
    },
    "directintstr_unorderedmap": {
     "3": "three",
     "42": "theanswer",
     "2": "two",
     "1": "one"
    },
    "directstrint_lut": {
     "one": 1,
     "theanswer": 42,
     "three": 3,
     "two": 2
    },
    "directstrint_map": {
     "one": 1,
     "theanswer": 42,
     "three": 3,
     "two": 2
    },
    "directstrint_unordered_map": {
     "theanswer": 42,
     "three": 3,
     "two": 2,
     "one": 1
    },
    "directstrobj_map": {
     "three": {
      "object": {
       "class": "SimpleTest",
       "uuid": "de4edcd0-a1c0-4ed6-8499-6c0dade9570e",
       "properties": {
        "value": "three"
       }
      }
     },
     "two": {
      "object": {
       "class": "SimpleTest",
       "uuid": "5c5ed6b9-b6bc-44f1-ac8a-5532fa25dd55",
       "properties": {
        "value": "two"
       }
      }
     },
     "yo": {
      "object": {
       "class": "SimpleTest",
       "uuid": "1f280b1e-677f-4b7c-b36d-6b40e09901be",
       "properties": {
        "value": "one"
       }
      }
     }
    }
   }
  }
 }
}
```

---

## Standard annotations for classes
  - **AssetSet** : **assetset_ptr_t** :  assetset for a given asset class
  - **ork.asset.loader** : **asset::loader_ptr_t** :  loader object for a given asset class
  - TODO : fix namespacing

## Standard Editor annotations for properties

  - **editor.visible** : **bool** :  is property displayable in UI editors ?
  - **editor.range** : **float_range or int_range** :  range for sliders in editor 
  - **editor.range.log** : **bool** :  range for sliders in editor shoud use log mode instead of linear mode 
  - **editor.range.min** : **bool** :  min val for sliders in editor
  - **editor.range.max** : **bool** :  max val for sliders in editor
  - **editor.range.precision** : **bool** :  number of digits to display for sliders in editor
  - **editor.factory.classbase** : **object::class_ptr_t** :  instantiable base class for object properties 
  - **editor.type** : **object::class_ptr_t** :  override editor widget type for a given property
  - TODO : fix namespacing

## Standard Python Binding annotations for properties

  - **python.visible** : **bool** :  is property visible from python bindings ?
  - TODO : fix namespacing

## Standard Serialization/DeSerialization (SerDes) annotations for properties

  - **reflect.no_instantiate** : **bool** :  typically for object maps or vectors - if true then contents of property (objects) are instantiated externally from reflection deserialization
  - TODO : fix namespacing


---

## python bindings

the reflection system is accessible from python
```python
graphdata = dflow.GraphData.createShared()
a = graphdata.create("A",dflow.LambdaModule)
b = graphdata.create("B",dflow.LambdaModule)

print("graphdata.uuid:", graphdata.uuid)
print("graphdata.properties:", graphdata.properties.dict)
```

would result in something like 
```bash
graphdata.uuid: c433e999-ac18-477e-84e9-8e5668fbbc85
graphdata.properties: {
  'Modules': {
    'A': LambdaModuleData(0x561f6345e790), 
    'B': LambdaModuleData(0x561f63484040)
  }
}
```

---

## ork.lev2 has example ImGui based editors for editing reflection based objects.

![Reflection Editor:1](ReflectionEditor.png)

