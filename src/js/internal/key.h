//
// Created by mkizub on 08.09.2026.
//

#pragma once

#ifndef EDROBOT_JS_KEY_H
#define EDROBOT_JS_KEY_H

namespace js::impl {

class key {
private:
#pragma pack(push, 1)
    union {
        struct {
            char buf[8 + 8 - 2];
            uint8_t idx;
            uint8_t sz;
        } s;
        struct {
            const char* ptr;
            uint32_t sz;
            uint8_t idx;
        } l;
    };
#pragma pack(pop)

public:
    key(unsigned idx, std::string_view sv) {
#ifndef NDEBUG
        std::memset((void*)this, 0, sizeof(*this));
#endif
        if (sv.empty()) {
            l.ptr = nullptr;
            l.sz = 0;
            s.sz = 0;
            s.idx = idx > 0xFF ? 0xFF : idx;
        }
        else if (sv.size() < sizeof(s.buf)) {
            s.sz = sv.size();
            strncpy_s(s.buf, sizeof(s.buf), sv.data(), sv.size());
            s.idx = idx > 0xFF ? 0xFF : idx;
        }
        else {
            s.sz = 255;
            auto k = gKeySet.emplace(sv);
            l.ptr = k.first->data();
            l.sz = sv.size();
            l.idx = idx > 0xFF ? 0xFF : idx;
        }
    }
    key(const key& other) {
        if (other.s.sz == 0) {
            l.ptr = nullptr;
            l.sz = 0;
            s.sz = 0;
            s.idx = other.s.idx;
        }
        else if (other.s.sz < sizeof(s.buf)) {
            s.sz = other.s.sz;
            s.idx = other.s.idx;
            strncpy_s(s.buf, sizeof(s.buf), other.s.buf, other.s.sz);
        }
        else {
            s.sz = 255;
            l.ptr = other.l.ptr;
            l.sz = other.l.sz;
            l.idx = other.l.idx;
        }
    }
    key(key&& other) {
        if (other.s.sz == 0) {
            l.ptr = nullptr;
            l.sz = 0;
            s.sz = 0;
            s.idx = other.s.idx;
        }
        else if (other.s.sz < sizeof(s.buf)) {
            s.sz = other.s.sz;
            s.idx = other.s.idx;
            strncpy_s(s.buf, sizeof(s.buf), other.s.buf, other.s.sz);
        }
        else {
            s.sz = 255;
            l.ptr = other.l.ptr;
            l.sz = other.l.sz;
            l.idx = other.l.idx;
        }
    }

    ~key() noexcept {}

    unsigned index() const noexcept {
        if (s.sz < sizeof(s.buf))
            return s.idx;
        else
            return l.idx;
    }

    explicit operator const char*() const {
        if (s.sz >= sizeof(s.buf))
            return l.ptr;
        return s.buf;
    }
    operator std::string_view() const {
        if (s.sz >= sizeof(s.buf))
            return {l.ptr, l.sz};
        return {s.buf, s.sz};
    }
    bool operator==(const key& other) const {
        return this->operator std::string_view() == other.operator std::string_view();
    }
    bool operator==(std::string_view other) const {
        return this->operator std::string_view() == other;
    }
    bool operator<(const key& other) const {
        const char* p1 = this->operator const char *();
        const char* p2 = other.operator const char *();
        return strcmp(p1, p2) < 0;
    }
    bool operator<(std::string_view other) const {
        const char* p1 = this->operator const char *();
        const char* p2 = other.data();
        return strcmp(p1, p2) < 0;
    }
};

} // namespace js::impl

#endif //EDROBOT_JS_KEY_H
