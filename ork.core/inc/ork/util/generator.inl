////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 

#include <coroutine>
#include <exception>
#include <iostream>

namespace ork::coroutine {
template<typename T>
struct generator {

    struct promise_type;

    using handle_type = std::coroutine_handle<promise_type>;

    generator(handle_type h) : handle(h) {}
    ~generator() {
        if (handle) handle.destroy();
    }

    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;

    generator(generator&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    generator& operator=(generator&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    class iterator {
    public:
        void operator++() {
            handle.resume();
            if (handle.done()) handle = nullptr;
        }

        const T& operator*() const {
            return handle.promise().current_value;
        }

        bool operator==(std::default_sentinel_t) const {
            return !handle || handle.done();
        }

        bool operator!=(std::default_sentinel_t s) const {
            return !(*this == s);
        }

        explicit iterator(handle_type h) : handle(h) {}

    private:
        handle_type handle;
    };

    iterator begin() {
        if (handle) handle.resume();
        return iterator{handle};
    }

    std::default_sentinel_t end() {
        return {};
    }

private:
    handle_type handle;
};

template<typename T>
struct generator<T>::promise_type {
    T current_value;

    auto get_return_object() {
        return generator{handle_type::from_promise(*this)};
    }

    std::suspend_always initial_suspend() {
        return {};
    }

    std::suspend_always final_suspend() noexcept {
        return {};
    }

    std::suspend_always yield_value(T value) {
        current_value = value;
        return {};
    }

    void return_void() {}
    void unhandled_exception() {
        std::exit(1); // or some other error handling
    }
};

} // namespace ork