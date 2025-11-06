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

    /// Get/set values by key using subscript syntax: vmap["key"] = timer
    public subscript(key: String) -> OrkidObject? {
        get {
            guard let valueHandle = orkid_varmap_get(handle, key) else {
                return nil
            }
            return OrkidObject(handle: valueHandle)
        }
        set {
            if let value = newValue {
                orkid_varmap_set(handle, key, value.handle)
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

    public mutating func next() -> (key: String, value: OrkidObject?)? {
        guard index < keys.count else { return nil }
        let key = keys[index]
        let value = varmap[key]
        index += 1
        return (key, value)
    }
}
