//
// Created by mkizub on 08.09.2026.
//

#pragma once

#ifndef EDROBOT_JS_STR_H
#define EDROBOT_JS_STR_H

namespace js {

class value;

namespace impl {

static const int str_buf_size = 16-1;
static const int val_type_off = 16-1;

class str {
private:
    friend class js::value;

    union {
        char buf[sizeof(void*)];
        const char *ptr;
    };

    inline void set_buf(std::string_view sv);
    inline void set_own(std::string_view sv);
    inline void set_ext(std::string_view sv);
    inline bool is_buf() const;
    inline bool is_own() const;
    inline bool is_ext() const;

    str(std::string_view sv) {
        if (sv.size() < str_buf_size)
            set_buf(sv);
        else if (auto* sym = js::symbol::FindString(sv))
            set_ext({sym->data(), sym->size()});
        else
            set_own(sv);
    }
public:
    str(const str& other) {
        if (other.size() < str_buf_size)
            set_buf(other.sv());
        else if (other.is_ext())
            set_ext(other.sv());
        else if (auto* sym = js::symbol::FindString(other.sv()))
            set_ext({sym->data(), sym->size()});
        else
            set_own(other.sv());
    }
    str(str&& other) {
        if (other.is_buf())
            set_buf(other.sv());
        else if (other.is_ext())
            set_ext(other.sv());
        else {
            set_own(other.sv());
            other.set_buf(std::string_view{});
        }
    }
    str& operator=(const str& other) {
        if (this == &other)
            return *this;
        if (other.size() < str_buf_size)
            set_buf(other.sv());
        else if (other.is_ext())
            set_ext(other.sv());
        else if (auto* sym = js::symbol::FindString(other.sv()))
            set_ext({sym->data(), sym->size()});
        else
            set_own(other.sv());
        return *this;
    }

    inline const char* data() const;
    inline uint32_t size() const;
    inline std::string_view sv() const;

    explicit operator const char*() const { return data(); }
    operator std::string_view() const { return sv(); }

    bool operator==(const str& other) const {
        return sv() == other.sv();
    }
    bool operator==(const std::string_view& other) const {
        return sv() == other;
    }
    bool operator<(const str& other) const {
        return sv() < other.sv();
    }
    bool operator<(std::string_view other) const {
        return sv() < other;
    }
    bool empty() const {
        return size() == 0;
    }
};

} // namespace impl
} // namespace js

#endif //EDROBOT_JS_STR_H
