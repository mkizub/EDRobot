//
// Created by mkizub on 18.09.2026.
//

#pragma once

#ifndef EDROBOT_JS_ARR_H
#define EDROBOT_JS_ARR_H

#include <cstdlib>
#include <algorithm>
#include <memory>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <iterator>
#include <assert.h>

namespace js::impl {

template <typename T>
class arr {
private:

    struct Storage {
        uint32_t m_size = 0;         // 4 bytes
        uint32_t m_capacity = 0;     // 4 bytes
        T m_data[];
        constexpr Storage(uint32_t capacity) {
            m_size = 0;
            m_capacity = capacity;
        }
        constexpr void push_back(const T& value) {
            assert (m_size < m_capacity);
            new (m_data + m_size) T(value);
            m_size += 1;
        }
        constexpr void push_back(T&& value) {
            assert (m_size < m_capacity);
            new (m_data + m_size) T(value);
            m_size += 1;
        }
    };
    Storage* m_storage = nullptr;

    constexpr uint32_t ext_capacity() const {
        if (!m_storage || m_storage->m_capacity == 0)
            return 2;
        if (m_storage->m_capacity < 2)
            return 4;
        return m_storage->m_capacity * 1.6;
    }

    void alloc_storage(uint32_t capacity) {
        m_storage = static_cast<Storage*>(std::malloc(sizeof(Storage) + capacity * sizeof(T)));
        if (!m_storage) throw std::bad_alloc();
        new (m_storage) Storage(capacity);
    }

public:
    using value_type = T;
    using size_type = uint32_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    arr() = default;

    explicit constexpr arr(uint32_t count, const T& value = T()) {
        if (count > 0) {
            alloc_storage(count);
            for (uint32_t i = 0; i < count; ++i)
                m_storage->push_back(value);
        }
    }

    constexpr arr(std::initializer_list<T> init) {
        if (init.size() > 0) {
            uint32_t len = static_cast<uint32_t>(init.size());
            alloc_storage(len);
            for (auto& item : init)
                m_storage->push_back(item);
        }
    }

    arr& operator=(std::initializer_list<T> init) {
        clear();
        std::free(m_storage);
        m_storage = nullptr;

        if (init.size() > 0) {
            uint32_t len = static_cast<uint32_t>(init.size());
            alloc_storage(len);
            for (auto& item : init)
                m_storage->push_back(item);
        }
        return *this;
    }

    constexpr ~arr() {
        clear();
        std::free(m_storage);
    }

    constexpr arr(const arr& other) {
        if (other.size()) {
            uint32_t len = other.size();
            alloc_storage(len);
            for (uint32_t i = 0; i < len; ++i)
                m_storage->push_back(other.m_storage->m_data[i]);
        }
    }

    arr& operator=(const arr& other) {
        if (this != &other) {
            clear();
            std::free(m_storage);
            m_storage = nullptr;
            if (other.size() > 0) {
                uint32_t len = other.size();
                alloc_storage(len);
                for (uint32_t i = 0; i < len; ++i)
                    m_storage->push_back(other.m_storage->m_data[i]);
            }
        }
        return *this;
    }

    constexpr arr(arr&& other) noexcept {
        m_storage = other.m_storage;
        other.m_storage = nullptr;
    }

    arr& operator=(arr&& other) noexcept {
        if (this != &other) {
            clear();
            std::free(m_storage);
            m_storage = other.m_storage;
            other.m_storage = nullptr;
        }
        return *this;
    }

    uint32_t size() const { return m_storage ? m_storage->m_size : 0; }
    uint32_t capacity() const { return m_storage ? m_storage->m_capacity : 0; }
    bool empty() const { return !m_storage || m_storage->m_size == 0; }

    T& operator[](uint32_t index) { return m_storage->m_data[index]; }
    const T& operator[](uint32_t index) const { return m_storage->m_data[index]; }

    T& at(uint32_t index) {
        if (!m_storage || index >= m_storage->m_size) throw std::out_of_range("js::impl::arr::at");
        return m_storage->m_data[index];
    }
    const T& at(uint32_t index) const {
        if (!m_storage || index >= m_storage->m_size) throw std::out_of_range("js::impl::arr::at");
        return m_storage->m_data[index];
    }

    T& front() { return m_storage->m_data; }
    const T& front() const { return m_storage->m_data; }
    T& back() { return m_storage->m_data[m_storage->m_size - 1]; }
    const T& back() const { return m_storage->m_data[m_storage->m_size - 1]; }

    T* data() { return m_storage ? m_storage->m_data : nullptr; }
    const T* data() const { return m_storage ? m_storage->m_data : nullptr; }

    iterator begin() { return m_storage ? m_storage->m_data : nullptr; }
    const_iterator begin() const { return m_storage ? m_storage->m_data : nullptr; }
    const_iterator cbegin() const { return m_storage ? m_storage->m_data : nullptr; }

    iterator end() { return m_storage ? m_storage->m_data + m_storage->m_size : nullptr; }
    const_iterator end() const { return m_storage ? m_storage->m_data + m_storage->m_size : nullptr; }
    const_iterator cend() const { return m_storage ? m_storage->m_data + m_storage->m_size : nullptr; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

    constexpr bool operator==(const arr& other) const {
        if (size() != other.size()) return false;
        return std::equal(begin(), end(), other.begin());
    }

    constexpr bool operator!=(const arr& other) const {
        return !(*this == other);
    }

    constexpr void reserve(uint32_t new_cap) {
        if (new_cap == 0) return;
        if (m_storage && new_cap <= m_storage->m_capacity) return;

        Storage* new_storage = static_cast<Storage*>(std::malloc(sizeof(Storage) + new_cap * sizeof(T)));
        if (!new_storage) throw std::bad_alloc();
        new (new_storage) Storage(new_cap);

        if (m_storage) {
            uint32_t len = m_storage->m_size;
            for (uint32_t i = 0; i < len; ++i)
                new_storage->push_back(std::move(m_storage->m_data[i]));
            free(m_storage);
        }
        m_storage = new_storage;
    }

    constexpr void push_back(const T& value) {
        if (size() == capacity())
            reserve(ext_capacity());
        m_storage->push_back(value);
    }

    constexpr void push_back(T&& value) {
        if (size() == capacity())
            reserve(ext_capacity());
        m_storage->push_back(std::move(value));
    }

    template <typename... Args>
    constexpr T& emplace_back(Args&&... args) {
        if (size() == capacity())
            reserve(ext_capacity());
        T* constructed_ptr = new (m_storage->m_data + m_storage->m_size) T(std::forward<Args>(args)...);
        ++m_storage->m_size;
        return *constructed_ptr;
    }

    // --- Insert Methods ---
    iterator insert(const_iterator pos, const T& value) {
        if (m_storage == nullptr) {
            if (pos == nullptr) {
                reserve(ext_capacity());
                push_back(value);
                return;
            }
            throw std::out_of_range("js::impl::arr::insert");
        }
        uint32_t index = static_cast<uint32_t>(pos - m_storage->m_data);
        if (index > size()) throw std::out_of_range("js::impl::arr::insert");

        if (m_storage->m_size == m_storage->m_capacity)
            reserve(ext_capacity());
        for (uint32_t i = 0; i < index; ++i)
            new (m_storage->new_data + i) T(std::move(m_storage->m_data[i]));
        new (m_storage->new_data + index) T(value);
        return m_storage->m_data + index;
    }

    iterator insert(const_iterator pos, T&& value) {
        if (m_storage == nullptr) {
            if (pos == nullptr) {
                reserve(ext_capacity());
                push_back(value);
                return;
            }
            throw std::out_of_range("js::impl::arr::insert");
        }
        uint32_t index = static_cast<uint32_t>(pos - m_storage->m_data);
        if (index > size()) throw std::out_of_range("js::impl::arr::insert");

        if (m_storage->m_size == m_storage->m_capacity)
            reserve(ext_capacity());
        for (uint32_t i = 0; i < index; ++i)
            new (m_storage->new_data + i) T(std::move(m_storage->m_data[i]));
        new (m_storage->new_data + index) T(std::move(value));
        return m_storage->m_data + index;
    }

    // --- Erase Methods ---
    iterator erase(const_iterator pos) {
        if (m_storage == nullptr) {
            if (pos == nullptr)
                return;
            throw std::out_of_range("js::impl::arr::erase");
        }
        uint32_t index = static_cast<uint32_t>(pos - m_storage->m_data);
        if (index >= m_storage->m_size) throw std::out_of_range("arr::erase");
        m_storage->m_data[index].~T();
        for (uint32_t i = index; i < m_storage->m_size - 1; ++i)
            new(m_storage->m_data + i) T(std::move(m_storage->m_data[i + 1]));
        --m_storage->m_size;
        return m_storage->m_data + index;
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (m_storage == nullptr) {
            if (first == nullptr && last == nullptr)
                return nullptr;
            throw std::out_of_range("arr::erase");
        }
        uint32_t first_idx = static_cast<uint32_t>(first - m_storage->m_data);
        uint32_t last_idx = static_cast<uint32_t>(last - m_storage->m_data);
        if (first_idx > last_idx || last_idx > m_storage->m_size) { throw std::out_of_range("js::impl::arr::erase"); }
        if (first_idx == last_idx) return m_storage->m_data + first_idx;
        uint32_t count = last_idx - first_idx;
        for (uint32_t i = first_idx; i < last_idx; ++i) { m_storage->m_data[i].~T(); }
        for (uint32_t i = first_idx; i < m_storage->m_size - count; ++i)
            new(m_storage->m_data + i) T(std::move(m_storage->m_data[i + count]));
        m_storage->m_size -= count;
        return m_storage->m_data + first_idx;
    }

    // ---------------------
    void pop_back() { if (size() > 0) { m_storage->m_data[--m_storage->m_size].~T(); }}

    void clear() {
        if (m_storage) {
            for (uint32_t i = 0; i < m_storage->m_size; ++i) { m_storage->m_data[i].~T(); }
            m_storage->m_size = 0;
        }
    }
};

} // namespace js::impl

#endif //EDROBOT_JS_ARR_H
