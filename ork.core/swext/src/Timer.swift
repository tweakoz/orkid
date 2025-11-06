// Timer - High-precision timer wrapper (facade over C++ Timer)
import Foundation

/// Swift wrapper for ork::Timer - INSTANTIABLE from Swift
public final class Timer: OrkidObject {

    // MARK: - Public Initialization (Swift can create!)

    public init() {
        super.init(handle: orkid_timer_create()!)
    }

    // MARK: - Internal init for wrapping C++-returned handles
    internal override init(handle: OpaquePointer, owned: Bool = true) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Timer Operations

    public func start() {
        orkid_timer_start(handle)
    }

    public func end() {
        orkid_timer_end(handle)
    }

    public var secsSinceStart: Float {
        return orkid_timer_secs_since_start(handle)
    }

    /// Get the global sync time
    public static var syncTime: Float {
        return orkid_timer_get_sync_time()
    }

    // MARK: - Convenience Methods

    /// Measure the execution time of a closure
    /// - Parameter block: The closure to measure
    /// - Returns: The elapsed time in seconds
    public func measure(_ block: () -> Void) -> Float {
        start()
        block()
        end()
        return secsSinceStart
    }

    /// Async version of measure for async closures
    /// - Parameter block: The async closure to measure
    /// - Returns: The elapsed time in seconds
    public func measure(_ block: () async -> Void) async -> Float {
        start()
        await block()
        end()
        return secsSinceStart
    }
}

// MARK: - CustomStringConvertible
extension Timer: CustomStringConvertible {
    public var description: String {
        return "Timer(elapsed: \(secsSinceStart)s)"
    }
}
