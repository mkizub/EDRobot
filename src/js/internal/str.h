//
// Created by mkizub on 08.09.2026.
//

#pragma once

#ifndef EDROBOT_JS_STR_H
#define EDROBOT_JS_STR_H

namespace js {
namespace impl {

class str {
private:
#pragma pack(push, 1)
    union {
        struct {
            char buf[7 + 8 + 8];
            uint8_t sz;
        } s;
        struct {
            const char *ptr;
            uint32_t sz;
        } l;
    };
#pragma pack(pop)

public:
    str(std::string_view sv) {
#ifndef NDEBUG
        std::memset((void*)this, 0, sizeof(*this));
#endif
        if (sv.empty()) {
            s.sz = 0;
            s.buf[0] = 0;
            l.ptr = 0;
        }
        else if (sv.size() < sizeof(s.buf)) {
            s.sz = sv.size();
            strncpy_s(s.buf, sizeof(s.buf), sv.data(), sv.size());
        }
        else if (const auto& it = gStrSet.find(sv); it != gStrSet.end()) {
            s.sz = 127;
            l.sz = it->size();
            l.ptr = it->data();
        }
        else {
            s.sz = 255;
            l.sz = sv.size();
            auto *tmp = (char *) malloc(sv.size() + 1);
            strncpy_s(tmp, sv.size() + 1, sv.data(), sv.size());
            l.ptr = tmp;
        }
    }
    str(const str& other) {
        if (other.s.sz == 0) {
            s.sz = 0;
            s.buf[0] = 0;
        }
        else if (other.s.sz < sizeof(s.buf)) {
            s.sz = other.s.sz;
            strncpy_s(s.buf, sizeof(s.buf), other.s.buf, other.s.sz);
        }
        else if (other.s.sz == 127) {
            s.sz = 127;
            l.sz = other.l.sz;
            l.ptr = other.l.ptr;
        }
        else {
            s.sz = 255;
            l.sz = other.l.sz;
            auto* tmp = (char*)malloc(l.sz + 1);
            strncpy_s(tmp, l.sz + 1, other.l.ptr, l.sz);
            l.ptr = tmp;
        }
    }
    str(str&& other) {
        if (other.s.sz == 0) {
            s.sz = 0;
            s.buf[0] = 0;
        }
        else if (other.s.sz < sizeof(s.buf)) {
            s.sz = other.s.sz;
            strncpy_s(s.buf, sizeof(s.buf), other.s.buf, other.s.sz);
        }
        else {
            s.sz = other.s.sz;
            l.sz = other.l.sz;
            l.ptr = other.l.ptr;
            other.s.sz = 0;
        }
    }
    str& operator=(const str& other) {
        if (this == &other)
            return *this;
        if (other.s.sz == 0) {
            s.sz = 0;
            s.buf[0] = 0;
        }
        else if (other.s.sz < sizeof(s.buf)) {
            s.sz = other.s.sz;
            strncpy_s(s.buf, sizeof(s.buf), other.s.buf, other.s.sz);
        }
        else if (other.s.sz == 127) {
            s.sz = 127;
            l.sz = other.l.sz;
            l.ptr = other.l.ptr;
        }
        else {
            s.sz = 255;
            l.sz = other.l.sz;
            auto* tmp = (char*)malloc(l.sz + 1);
            strncpy_s(tmp, l.sz + 1, other.l.ptr, l.sz);
            l.ptr = tmp;
        }
        return *this;
    }

    ~str() noexcept {
        if (s.sz == 255)
            free((void *) l.ptr);
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
    bool operator==(const str& other) const {
        return this->operator std::string_view() == other.operator std::string_view();
    }
    bool operator==(const std::string_view& other) const {
        return this->operator std::string_view() == other;
    }
    bool operator<(const str& other) const {
        const char* p1 = this->operator const char *();
        const char* p2 = other.operator const char *();
        return strcmp(p1, p2) < 0;
    }
    bool operator<(std::string_view other) const {
        const char* p1 = this->operator const char *();
        const char* p2 = other.data();
        return strcmp(p1, p2) < 0;
    }
    bool empty() const {
        return s.sz == 0;
    }
};

} // namespace impl
} // namespace js

#endif //EDROBOT_JS_STR_H
