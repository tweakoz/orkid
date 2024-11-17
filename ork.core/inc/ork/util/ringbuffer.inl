////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 

namespace ork {

// no frills unbounded ring buffer

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t size)
        : _buffer(size), _size(size), _read_index(0), _write_index(0), _count(0) {}

    void push_one(const T& value) {
        if (_count == _size) {
            // Buffer is full, overwrite oldest data
            _buffer[_write_index] = value;
            _write_index = (_write_index + 1) % _size;
            _read_index = _write_index; // Move read index to the next oldest data
        } else {
            _buffer[_write_index] = value;
            _write_index = (_write_index + 1) % _size;
            ++_count;
        }
    }

    void push_many(const T* values, size_t count) {
        if (count >= _size) {
            // Only keep the last _size elements
            values += count - _size;
            count = _size;
        }
        if (count > _size - _count) {
            // Overwrite oldest data
            size_t overwrite_count = count - (_size - _count);
            _read_index = (_read_index + overwrite_count) % _size;
            _count = _size;
        } else {
            _count += count;
        }

        size_t first_chunk = std::min(count, _size - _write_index);
        std::copy(values, values + first_chunk, _buffer.begin() + _write_index);
        size_t second_chunk = count - first_chunk;
        if (second_chunk > 0) {
            std::copy(values + first_chunk, values + count, _buffer.begin());
        }
        _write_index = (_write_index + count) % _size;
    }

    T pop_one() {
        if (_count == 0) {
            throw std::runtime_error("RingBuffer is empty");
        }
        T value = _buffer[_read_index];
        _read_index = (_read_index + 1) % _size;
        --_count;
        return value;
    }

    void pop_many(T* values, size_t count) {
        if (count > _count) {
            throw std::runtime_error("Not enough data in RingBuffer");
        }
        size_t first_chunk = std::min(count, _size - _read_index);
        std::copy(_buffer.begin() + _read_index, _buffer.begin() + _read_index + first_chunk, values);
        size_t second_chunk = count - first_chunk;
        if (second_chunk > 0) {
            std::copy(_buffer.begin(), _buffer.begin() + second_chunk, values + first_chunk);
        }
        _read_index = (_read_index + count) % _size;
        _count -= count;
    }

    void pop_many(std::vector<T>& output, size_t count) {
        if (count > _count) {
            throw std::runtime_error("Not enough data in RingBuffer");
        }
        size_t first_chunk = std::min(count, _size - _read_index);
        output.insert(output.end(), _buffer.begin() + _read_index, _buffer.begin() + _read_index + first_chunk);
        size_t second_chunk = count - first_chunk;
        if (second_chunk > 0) {
            output.insert(output.end(), _buffer.begin(), _buffer.begin() + second_chunk);
        }
        _read_index = (_read_index + count) % _size;
        _count -= count;
    }

    size_t size() const {
        return _count;
    }

private:
    std::vector<T> _buffer;
    size_t _size;
    size_t _read_index;
    size_t _write_index;
    size_t _count;
};

} // namespace ork