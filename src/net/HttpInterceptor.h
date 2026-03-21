//
// Created by mkizub on 21.03.2026.
//

#pragma once

#ifndef EDROBOT_HTTPINTERCEPTOR_H
#define EDROBOT_HTTPINTERCEPTOR_H

#include <cpr/cpr.h>

class HttpInterceptor : public cpr::Interceptor {
    const int reqId;
    cpr::ConnectionPool* pool;
    std::string_view mode;
public:
    HttpInterceptor(cpr::Session& session);
    cpr::Response intercept(cpr::Session& session) override;
};

namespace cpr::priv {

template <>
inline void set_option_internal<false, Url>(Session& session, Url&& url) {
    session.SetUrl(std::forward<Url>(url));
    session.AddInterceptor(std::shared_ptr<cpr::Interceptor>(new HttpInterceptor(session)));
}

} //namespace cpr::priv

inline bool isOK(cpr::Response& cr) {
    return cr.status_code > 0 && cr.status_code < 400 ;
}

extern js::value getJS(cpr::Response& cr);

#endif //EDROBOT_HTTPINTERCEPTOR_H
