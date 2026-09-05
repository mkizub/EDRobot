//
// Created by mkizub on 04.09.2026.
//

#pragma once

#ifndef EDROBOT_COMPACT_VECTOR_H
#define EDROBOT_COMPACT_VECTOR_H

#include <cstdlib>
#include <algorithm>
#include <memory>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <iterator>

template <typename T>
class compact_vector {
private:
    T* m_data = nullptr;         // 8 bytes on 64-bit systems
    uint32_t m_size = 0;         // 4 bytes
    uint32_t m_capacity = 0;     // 4 bytes
    // Total sizeof(compact_vector<T>) == 16 bytes

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

    compact_vector() = default;

    explicit compact_vector(uint32_t count, const T& value = T()) {
        if (count > 0) {
            m_data = static_cast<T*>(std::malloc(count * sizeof(T)));
            if (!m_data) throw std::bad_alloc();
            for (uint32_t i = 0; i < count; ++i) {
                new (m_data + i) T(value);
            }
            m_size = count;
            m_capacity = count;
        }
    }

    compact_vector(std::initializer_list<T> init) {
        if (init.size() > 0) {
            uint32_t len = static_cast<uint32_t>(init.size());
            m_data = static_cast<T*>(std::malloc(len * sizeof(T)));
            if (!m_data) throw std::bad_alloc();

            uint32_t i = 0;
            for (auto& item : init) {
                new (m_data + i) T(item);
                ++i;
            }
            m_size = len;
            m_capacity = len;
        }
    }

    compact_vector& operator=(std::initializer_list<T> init) {
        clear();
        std::free(m_data);
        m_data = nullptr;
        m_size = m_capacity = 0;

        if (init.size() > 0) {
            uint32_t len = static_cast<uint32_t>(init.size());
            m_data = static_cast<T*>(std::malloc(len * sizeof(T)));
            if (!m_data) throw std::bad_alloc();

            uint32_t i = 0;
            for (auto& item : init) {
                new (m_data + i) T(item);
                ++i;
            }
            m_size = len;
            m_capacity = len;
        }
        return *this;
    }

    ~compact_vector() {
        clear();
        std::free(m_data);
    }

    compact_vector(const compact_vector& other) {
        if (other.m_size > 0) {
            m_data = static_cast<T*>(std::malloc(other.m_size * sizeof(T)));
            if (!m_data) throw std::bad_alloc();
            for (uint32_t i = 0; i < other.m_size; ++i) {
                new (m_data + i) T(other.m_data[i]);
            }
            m_size = other.m_size;
            m_capacity = other.m_size;
        }
    }

    compact_vector& operator=(const compact_vector& other) {
        if (this != &other) {
            clear();
            std::free(m_data);
            m_data = nullptr;
            m_size = m_capacity = 0;

            if (other.m_size > 0) {
                m_data = static_cast<T*>(std::malloc(other.m_size * sizeof(T)));
                if (!m_data) throw std::bad_alloc();
                for (uint32_t i = 0; i < other.m_size; ++i) {
                    new (m_data + i) T(other.m_data[i]);
                }
                m_size = other.m_size;
                m_capacity = other.m_size;
            }
        }
        return *this;
    }

    compact_vector(compact_vector&& other) noexcept
            : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    compact_vector& operator=(compact_vector&& other) noexcept {
        if (this != &other) {
            clear();
            std::free(m_data);

            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    uint32_t size() const { return m_size; }
    uint32_t capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

    T& operator[](uint32_t index) { return m_data[index]; }
    const T& operator[](uint32_t index) const { return m_data[index]; }

    T& at(uint32_t index) {
        if (index >= m_size) throw std::out_of_range("compact_vector::at");
        return m_data[index];
    }
    const T& at(uint32_t index) const {
        if (index >= m_size) throw std::out_of_range("compact_vector::at");
        return m_data[index];
    }

    T& front() { return m_data; }
    const T& front() const { return m_data; }
    T& back() { return m_data[m_size - 1]; }
    const T& back() const { return m_data[m_size - 1]; }

    T* data() { return m_data; }
    const T* data() const { return m_data; }

    iterator begin() { return m_data; }
    const_iterator begin() const { return m_data; }
    const_iterator cbegin() const { return m_data; }

    iterator end() { return m_data + m_size; }
    const_iterator end() const { return m_data + m_size; }
    const_iterator cend() const { return m_data + m_size; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

    bool operator==(const compact_vector& other) const {
        if (m_size != other.m_size) return false;
        return std::equal(begin(), end(), other.begin());
    }

    bool operator!=(const compact_vector& other) const {
        return !(*this == other);
    }

    void reserve(uint32_t new_cap) {
        if (new_cap <= m_capacity) return;

        T* new_data = static_cast<T*>(std::malloc(new_cap * sizeof(T)));
        if (!new_data) throw std::bad_alloc();

        for (uint32_t i = 0; i < m_size; ++i) {
            new (new_data + i) T(std::move(m_data[i]));
            m_data[i].~T();
        }
        std::free(m_data);
        m_data = new_data;
        m_capacity = new_cap;
    }

    void push_back(const T& value) {
        if (m_size == m_capacity) {
            reserve(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        new (m_data + m_size) T(value);
        ++m_size;
    }

    void push_back(T&& value) {
        if (m_size == m_capacity) {
            reserve(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        new (m_data + m_size) T(std::move(value));
        ++m_size;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (m_size == m_capacity) {
            reserve(m_capacity == 0 ? 4 : m_capacity * 2);
        }
        T* constructed_ptr = new (m_data + m_size) T(std::forward<Args>(args)...);
        ++m_size;
        return *constructed_ptr;
    }

    // --- Insert Methods ---
    iterator insert(const_iterator pos, const T& value) {
        uint32_t index = static_cast<uint32_t>(pos - m_data);
        if (index > m_size) throw std::out_of_range("compact_vector::insert");

        if (m_size == m_capacity) {
            uint32_t new_cap = (m_capacity == 0 ? 4 : m_capacity * 2);
            T* new_data = static_cast<T*>(std::malloc(new_cap * sizeof(T)));
            if (!new_data) throw std::bad_alloc();

            for (uint32_t i = 0; i < index; ++i) {
                new (new_data + i) T(std::move(m_data[i]));
                m_data[i].~T();
            }
            new (new_data + index) T(value);
            for (uint32_t i = index; i < m_size; ++i) {
                new (new_data + i + 1) T(std::move(m_data[i]));
                m_data[i].~T();
            }
            std::free(m_data);
            m_data = new_data;
            m_capacity = new_cap;
        } else {
            if (m_size > 0) {
                new (m_data + m_size) T(std::move(m_data[m_size - 1]));
                for (uint32_t i = m_size - 1; i > index; --i) {
                    m_data[i] = std::move(m_data[i - 1]);
                }
                m_data[index] = value;
            } else {
                new (m_data) T(value);
            }
        }
        ++m_size;
        return m_data + index;
    }

    iterator insert(const_iterator pos, T&& value) {
        uint32_t index = static_cast<uint32_t>(pos - m_data);
        if (index > m_size) throw std::out_of_range("compact_vector::insert");

        if (m_size == m_capacity) {
            uint32_t new_cap = (m_capacity == 0 ? 4 : m_capacity * 2);
            T* new_data = static_cast<T*>(std::malloc(new_cap * sizeof(T)));
            if (!new_data) throw std::bad_alloc();

            for (uint32_t i = 0; i < index; ++i) {
                new (new_data + i) T(std::move(m_data[i]));
                m_data[i].~T();
            }
            new (new_data + index) T(std::move(value));
            for (uint32_t i = index; i < m_size; ++i) {
                new (new_data + i + 1) T(std::move(m_data[i]));
                m_data[i].~T();
            }
            std::free(m_data);
            m_data = new_data;
            m_capacity = new_cap;
        } else {
            if (m_size > 0) {
                new (m_data + m_size) T(std::move(m_data[m_size - 1]));
                for (uint32_t i = m_size - 1; i > index; --i) {
                    m_data[i] = std::move(m_data[i - 1]);
                }
                m_data[index] = std::move(value);
            } else {
                new (m_data) T(std::move(value));
            }
        }
        ++m_size;
        return m_data + index;
    }
    // ----------------------
    // --- Erase Methods ---
    iterator erase(const_iterator pos) {
        uint32_t index = static_cast<uint32_t>(pos - m_data);
        if (index >= m_size) throw std::out_of_range("compact_vector::erase");
        m_data[index].~T();
        for (uint32_t i = index; i < m_size - 1; ++i) {
            new(m_data + i) T(std::move(m_data[i + 1]));
            m_data[i + 1].~T();
        }
        --m_size;
        return m_data + index;
    }

    iterator erase(const_iterator first, const_iterator last) {
        uint32_t first_idx = static_cast<uint32_t>(first - m_data);
        uint32_t last_idx = static_cast<uint32_t>(last - m_data);
        if (first_idx > last_idx || last_idx > m_size) { throw std::out_of_range("compact_vector::erase"); }
        if (first_idx == last_idx) return m_data + first_idx;
        uint32_t count = last_idx - first_idx;
        for (uint32_t i = first_idx; i < last_idx; ++i) { m_data[i].~T(); }
        for (uint32_t i = first_idx; i < m_size - count; ++i) {
            new(m_data + i) T(std::move(m_data[i + count]));
            m_data[i + count].~T();
        }
        m_size -= count;
        return m_data + first_idx;
    }
    // ---------------------
    void pop_back() { if (m_size > 0) { m_data[--m_size].~T(); }}

    void clear() {
        for (uint32_t i = 0; i < m_size; ++i) { m_data[i].~T(); }
        m_size = 0;
    }
};

#endif //EDROBOT_COMPACT_VECTOR_H
