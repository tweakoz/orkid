// VarMap - Dictionary-like container for Orkid objects (facade over C++ varmap::VarMap)
import Foundation

/// Swift wrapper for ork::varmap::VarMap - INSTANTIABLE from Swift
public final class VarMap: OrkidObject {

    // MARK: - Public Initialization (Swift can create!)

    public init() {
        super.init(handle: orkid_varmap_create()!)
    }

    // MARK: - Internal init for wrapping C++-returned handles
    internal override init(handle: OpaquePointer, owned: Bool = true) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Subscript Access

    /// Get/set values by key using subscript syntax
    /// Getter returns OrkidObject (cast as needed)
    /// Setter accepts OrkidObject or raw closures (auto-wrapped)
    /// Usage:
    ///   vmap["timer"] = Timer()
    ///   vmap["callback"] = { print("Clicked!") }
    public subscript(key: String) -> Any? {
        get {
            guard let valueHandle = orkid_varmap_get(handle, key) else {
                return nil
            }
            return OrkidObject(handle: valueHandle)
        }
        set {
            if let value = newValue {
                if let obj = value as? OrkidObject {
                    // Direct OrkidObject storage
                    orkid_varmap_set(handle, key, obj.handle)
                } else if let closure = value as? (Int) -> Void {
                    // Typed closure: (Int) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback1(closure: { arg in
                        closure(arg as! Int)  // Asserts in C++ decode if wrong type
                    }).handle)
                } else if let closure = value as? (Float) -> Void {
                    // Typed closure: (Float) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback1(closure: { arg in
                        closure(arg as! Float)
                    }).handle)
                } else if let closure = value as? (Double) -> Void {
                    // Typed closure: (Double) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback1(closure: { arg in
                        closure(arg as! Double)
                    }).handle)
                } else if let closure = value as? (String) -> Void {
                    // Typed closure: (String) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback1(closure: { arg in
                        closure(arg as! String)
                    }).handle)
                } else if let closure = value as? (OrkidObject) -> Void {
                    // Typed closure: (OrkidObject) -> Void (handles Timer, vec3, etc.)
                    orkid_varmap_set(handle, key, SwiftCallback1(closure: { arg in
                        closure(arg as! OrkidObject)
                    }).handle)
                } else if let closure = value as? (Any) -> Void {
                    // Generic 1-arg closure: (Any) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback1(closure: closure).handle)
                } else if let closure = value as? (Any, Any) -> Void {
                    // Generic 2-arg closure: (Any, Any) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback2(closure: closure).handle)
                } else if let closure = value as? (Any, Any, Any) -> Void {
                    // Generic 3-arg closure: (Any, Any, Any) -> Void
                    orkid_varmap_set(handle, key, SwiftCallback3(closure: closure).handle)
                } else if let closure = value as? () -> Void {
                    // 0-arg closure: () -> Void
                    orkid_varmap_set(handle, key, SwiftCallback(closure: closure).handle)
                }
                // Ignore other types
            } else {
                orkid_varmap_remove(handle, key)
            }
        }
    }

    // MARK: - Dictionary Operations

    /// Check if key exists
    public func contains(_ key: String) -> Bool {
        return orkid_varmap_contains(handle, key)
    }

    /// Remove value for key
    public func remove(_ key: String) {
        orkid_varmap_remove(handle, key)
    }

    /// Get all keys
    public var keys: [String] {
        var count: Int32 = 0
        guard let keysPtr = orkid_varmap_keys(handle, &count) else {
            return []
        }

        var result: [String] = []
        result.reserveCapacity(Int(count))

        for i in 0..<Int(count) {
            if let cstr = keysPtr[i] {
                result.append(String(cString: cstr))
            }
        }

        orkid_varmap_free_keys(keysPtr, count)
        return result
    }

    /// Number of entries
    public var count: Int {
        return Int(orkid_varmap_size(handle))
    }

    /// Check if empty
    public var isEmpty: Bool {
        return count == 0
    }

    /// Remove all entries
    public func clear() {
        orkid_varmap_clear(handle)
    }

    /// Create a copy
    public func clone() -> VarMap {
        return VarMap(handle: orkid_varmap_clone(handle)!)
    }

    /// Invoke a callback stored in the VarMap by key (no arguments)
    /// The value at the key must be a SwiftCallback
    public func invokeNamedCallback(_ key: String) {
        orkid_varmap_invoke_callback(handle, key)
    }

    /// Invoke a callback with 1 argument
    /// The value at the key must be a SwiftCallback1
    /// Argument can be: primitives (Int, Float, String), OrkidObjects (Timer, vec3, etc.)
    public func invokeNamedCallback(_ key: String, _ arg: Any) {
        let encoded = encodeSwiftValue(arg)
        orkid_varmap_invoke_callback_1arg(handle, key, encoded)
    }

    /// Invoke a callback with 2 arguments
    /// The value at the key must be a SwiftCallback2
    public func invokeNamedCallback(_ key: String, _ arg1: Any, _ arg2: Any) {
        let encoded1 = encodeSwiftValue(arg1)
        let encoded2 = encodeSwiftValue(arg2)
        orkid_varmap_invoke_callback_2arg(handle, key, encoded1, encoded2)
    }

    /// Invoke a callback with 3 arguments
    /// The value at the key must be a SwiftCallback3
    public func invokeNamedCallback(_ key: String, _ arg1: Any, _ arg2: Any, _ arg3: Any) {
        let encoded1 = encodeSwiftValue(arg1)
        let encoded2 = encodeSwiftValue(arg2)
        let encoded3 = encodeSwiftValue(arg3)
        orkid_varmap_invoke_callback_3arg(handle, key, encoded1, encoded2, encoded3)
    }

    /// Helper to encode Swift values to OrkidHandleBase*
    private func encodeSwiftValue(_ value: Any) -> OpaquePointer {
        if let obj = value as? OrkidObject {
            return obj.handle
        } else if let intVal = value as? Int {
            return orkid_encode_int(Int32(intVal))!
        } else if let floatVal = value as? Float {
            return orkid_encode_float(floatVal)!
        } else if let doubleVal = value as? Double {
            return orkid_encode_double(doubleVal)!
        } else if let strVal = value as? String {
            return orkid_encode_string(strVal)!
        }
        // Fallback: return null handle (will assert in C++)
        fatalError("Cannot encode type \(type(of: value)) - not a supported primitive or OrkidObject")
    }
}

// MARK: - CustomStringConvertible
extension VarMap: CustomStringConvertible {
    public var description: String {
        return "VarMap(count: \(count))"
    }
}

// MARK: - Sequence (for-in support)
extension VarMap: Sequence {
    public func makeIterator() -> VarMapIterator {
        return VarMapIterator(keys: keys, varmap: self)
    }
}

public struct VarMapIterator: IteratorProtocol {
    private var keys: [String]
    private var index: Int = 0
    private let varmap: VarMap

    init(keys: [String], varmap: VarMap) {
        self.keys = keys
        self.varmap = varmap
    }

    public mutating func next() -> (key: String, value: Any?)? {
        guard index < keys.count else { return nil }
        let key = keys[index]
        let value = varmap[key]
        index += 1
        return (key, value)
    }
}
